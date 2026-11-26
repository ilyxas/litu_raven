#include "raven_services/peer_service.hpp"
#include <cassert>
#include <cstdio>

namespace raven::services {
using namespace comms;
using diagnostics::Event;
namespace {
constexpr uint64_t semantic_timeout_ms = 3000;
constexpr uint64_t hello_retry_ms = 1000;
constexpr uint64_t heartbeat_interval_ms = 1000;
constexpr uint64_t passive_hello_timeout_ms = 5000;
constexpr unsigned hello_attempt_limit = 3;
} // namespace
PeerService::PeerService(CommunicationPort &port, runtime::Dispatcher &dispatcher,
                         const icd::DeviceDescriptor &local, Clock clock, PeerSlot *slots, PeerRecord *peers,
                         size_t peer_capacity, diagnostics::Journal &journal, uint8_t endpoint)
    : port_(port), dispatcher_(dispatcher), local_(local), clock_(clock), slots_(slots), peers_(peers),
      peer_capacity_(peer_capacity), journal_(journal), diagnostics_endpoint_(endpoint) {
    for (uint16_t id = uint16_t(icd::Type::HelloRequest); id <= uint16_t(icd::Type::Receipt); ++id) {
        const bool ok = inbox.subscribe(incoming(icd::Type(id)), sizeof(Inbound), runtime::Policy::Ordered);
        assert(ok);
        (void)ok;
    }
    const bool ok = inbox.subscribe(probe_request_id, sizeof(ProbeRequest), runtime::Policy::Ordered);
    assert(ok);
    (void)ok;
}
const char *name(PeerStatus s) {
    switch (s) {
    case PeerStatus::Unknown:
        return "unknown";
    case PeerStatus::Handshaking:
        return "handshaking";
    case PeerStatus::NotReady:
        return "not-ready";
    case PeerStatus::Ready:
        return "ready";
    case PeerStatus::Disconnected:
        return "disconnected";
    }
    return "invalid";
}
const char *name(Outcome o) {
    switch (o) {
    case Outcome::Pending:
        return "pending";
    case Outcome::Succeeded:
        return "success";
    case Outcome::LocalRejected:
        return "local-rejected";
    case Outcome::TransportLost:
        return "transport-lost";
    case Outcome::SendDeadline:
        return "send-deadline";
    case Outcome::RemoteNack:
        return "remote-nack";
    case Outcome::ResponseTimeout:
        return "response-timeout";
    case Outcome::Rejected:
        return "rejected";
    }
    return "invalid";
}
const char *name(Failure f) {
    switch (f) {
    case Failure::None:
        return "none";
    case Failure::TransportLost:
        return "transport-lost";
    case Failure::Protocol:
        return "protocol";
    case Failure::LocalFull:
        return "local-full";
    case Failure::SendDeadline:
        return "send-deadline";
    case Failure::WriteError:
        return "write-error";
    case Failure::ResponseTimeout:
        return "response-timeout";
    case Failure::RemoteNack:
        return "remote-nack";
    case Failure::IdentityConflict:
        return "identity-conflict";
    }
    return "invalid";
}
bool PeerService::peer(size_t index, PeerRecord &value) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (index >= peer_capacity_ || !peers_[index].descriptor.device)
        return false;
    value = peers_[index];
    if (value.status != PeerStatus::Disconnected) {
        auto *session = port_.session(value.route);
        if (!session || !session->active(value.route))
            value.status = PeerStatus::Disconnected;
    }
    return true;
}
uint32_t PeerService::stale_messages() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return stale_;
}
runtime::Delivery PeerService::probe(icd::DeviceId id, uint32_t generation, uint32_t request) {
    return inbox.post(
        runtime::Message::make(probe_request_id, request, ProbeRequest{id, generation, request}));
}
bool PeerService::register_peer(PeerSlot &slot, const icd::DeviceDescriptor &descriptor, uint64_t now) {
    if (!descriptor.device || descriptor.device == local_.device || descriptor.protocol != icd::version ||
        descriptor.readiness == icd::Readiness::Rejected)
        return false;
    // Identity of an established transport binding is immutable.
    if (slot.established) {
        const auto &old = peers_[slot.peer_index].descriptor;
        return old.device == descriptor.device && old.boot == descriptor.boot;
    }
    size_t selected = SIZE_MAX;
    for (size_t i = 0; i < peer_capacity_; ++i) {
        if (peers_[i].descriptor.device != descriptor.device)
            continue;
        if (peers_[i].status != PeerStatus::Disconnected && peers_[i].route != slot.route)
            return false;
        selected = i;
        break;
    }
    if (selected == SIZE_MAX) {
        for (size_t i = 0; i < peer_capacity_; ++i) {
            if (!peers_[i].descriptor.device) {
                selected = i;
                break;
            }
            if (peers_[i].status == PeerStatus::Disconnected &&
                (selected == SIZE_MAX || peers_[i].changed_at < peers_[selected].changed_at))
                selected = i;
        }
    }
    if (selected == SIZE_MAX)
        return false;
    peers_[selected] = {descriptor, slot.route, now, now, PeerStatus::Handshaking, Failure::None};
    slot.peer_index = selected;
    slot.established = true;
    slot.next_heartbeat = now + heartbeat_interval_ms;
    readiness(slot, descriptor.readiness, descriptor.reason, now);
    return true;
}
void PeerService::readiness(PeerSlot &slot, icd::Readiness ready, icd::Reason reason, uint64_t now) {
    if (!slot.established)
        return;
    auto &peer = peers_[slot.peer_index];
    const auto state = ready == icd::Readiness::Ready ? PeerStatus::Ready : PeerStatus::NotReady;
    peer.last_seen = now;
    peer.descriptor.readiness = ready;
    peer.descriptor.reason = reason;
    if (state == peer.status)
        return;
    peer.status = state;
    peer.changed_at = now;
    journal_.append(state == PeerStatus::Ready ? Event::PeerReady : Event::PeerNotReady, now,
                    peer.descriptor.device, peer.descriptor.boot, name(state));
}
void PeerService::complete(PeerSlot &slot, PendingExchange &pending, Outcome result, uint64_t now) {
    if (!pending.active)
        return;
    pending.active = false;
    pending.outcome = result;
    auto *session = port_.session(slot.route);
    if (session && pending.ticket)
        session->cancel(pending.ticket);
    const auto id = slot.established ? peers_[slot.peer_index].descriptor.device : 0;
    const auto boot = slot.established ? peers_[slot.peer_index].descriptor.boot : 0;
    if (pending.manual) {
        const ProbeResult reply{id, pending.has_written ? now - pending.first_written : 0,
                                pending.console_generation, pending.console_request, result};
        const auto delivered =
            dispatcher_.deliver(runtime::destination(diagnostics_endpoint_),
                                runtime::Message::make(probe_result_id, pending.console_request, reply));
        journal_.append(Event::ProbeCompleted, now, id, boot,
                        delivered.complete() ? name(result) : "probe complete; console busy");
    } else if (result != Outcome::Succeeded) {
        journal_.append(Event::ExchangeFailed, now, id, boot, name(result), 2);
    }
}
void PeerService::attempt(PeerSlot &slot, PendingExchange &pending, uint64_t now) {
    auto &session = port_.at(slot.route.slot);
    const auto ticket = session.reserve(slot.route, pending.request.exchange, pending.request.type, now);
    if (!ticket) {
        complete(slot, pending, Outcome::LocalRejected, now);
        return;
    }
    pending.ticket = ticket;
    ++pending.attempts;
    const auto posted = port_.send(slot.route, pending.request, ticket);
    if (posted != Post::Accepted) {
        session.cancel(ticket);
        session.release(ticket);
        pending.ticket = {};
        complete(slot, pending, posted == Post::StaleRoute ? Outcome::TransportLost : Outcome::LocalRejected,
                 now);
    }
}
void PeerService::start_exchange(PeerSlot &slot, PendingExchange &pending, icd::Type type, uint64_t now,
                                 const ProbeRequest *probe) {
    if (pending.active || pending.ticket)
        return;
    if (exchange_counter_ == UINT32_MAX) {
        port_.at(slot.route.slot).close(slot.route, Failure::Protocol);
        return;
    }
    pending = {};
    pending.active = true;
    pending.outcome = Outcome::Pending;
    pending.request.type = type;
    pending.request.exchange = ++exchange_counter_;
    pending.request.requires_ack = type == icd::Type::HelloRequest;
    if (type == icd::Type::HelloRequest)
        pending.request.body.set(icd::Hello{local_, now});
    else
        pending.request.body.set(icd::Heartbeat{now, now, local_.reason, local_.readiness, 0});
    if (probe) {
        pending.manual = true;
        pending.console_generation = probe->console_generation;
        pending.console_request = probe->request;
    }
    attempt(slot, pending, now);
}
void PeerService::advance(PeerSlot &slot, PendingExchange &p, uint64_t now) {
    auto &session = port_.at(slot.route.slot);
    if (p.ticket) {
        const auto progress = session.progress(p.ticket);
        if (progress.phase == SendPhase::Written && p.active && !p.has_written) {
            p.has_written = true;
            p.first_written = progress.finished;
            p.deadline = p.first_written + semantic_timeout_ms;
            p.next_retry = p.first_written + hello_retry_ms;
        }
        if (progress.phase == SendPhase::Failed && p.active)
            complete(slot, p,
                     progress.reason == Failure::SendDeadline ? Outcome::SendDeadline
                                                              : Outcome::TransportLost,
                     now);
        if (terminal(progress.phase)) {
            session.release(p.ticket);
            p.ticket = {};
        } else if (p.active && now >= progress.admitted + Session::local_send_budget_ms) {
            complete(slot, p, Outcome::SendDeadline, now);
            session.close(slot.route, Failure::SendDeadline);
        }
    }
    if (!p.active)
        return;
    if (p.has_written && now >= p.deadline) {
        complete(slot, p, Outcome::ResponseTimeout, now);
        return;
    }
    if (p.has_written && p.request.type == icd::Type::HelloRequest && !p.ticket &&
        p.attempts < hello_attempt_limit && now >= p.next_retry) {
        p.next_retry = p.first_written + uint64_t(p.attempts + 1) * hello_retry_ms;
        attempt(slot, p, now);
    }
}
void PeerService::open_slot(PeerSlot &slot, SessionKey route, uint64_t now) {
    slot = {};
    slot.route = route;
    char text[64]{};
    std::snprintf(text, sizeof(text), "open slot=%lu generation=%lu", (unsigned long)route.slot,
                  (unsigned long)route.generation);
    journal_.append(Event::LinkOpened, now, 0, 0, text);
}
void PeerService::close_slot(PeerSlot &slot, Failure failure, uint64_t now) {
    for (auto &pending : slot.exchanges)
        complete(slot, pending, Outcome::TransportLost, now);
    icd::DeviceId id = 0;
    icd::BootId boot = 0;
    if (slot.established) {
        auto &peer = peers_[slot.peer_index];
        peer.status = PeerStatus::Disconnected;
        peer.failure = failure;
        peer.changed_at = now;
        id = peer.descriptor.device;
        boot = peer.descriptor.boot;
    }
    char text[80]{};
    std::snprintf(text, sizeof(text), "%s slot=%lu generation=%lu", name(failure),
                  (unsigned long)slot.route.slot, (unsigned long)slot.route.generation);
    journal_.append(Event::LinkLost, now, id, boot, text, 2);
    slot.established = false;
}
void PeerService::on_tick(uint64_t) {
    const auto now = clock_.now();
    std::lock_guard<std::mutex> state(state_mutex_);
    for (size_t i = 0; i < port_.count(); ++i) {
        auto &session = port_.at(i);
        auto snapshot = session.snapshot();
        auto &slot = slots_[i];
        if (snapshot.phase == Phase::Closing) {
            // Close is reliable state, not an event that can be lost to a full inbox.
            if (!snapshot.peer_retired) {
                if (slot.route != snapshot.key) {
                    slot = {};
                    slot.route = snapshot.key;
                }
                close_slot(slot, snapshot.failure, now);
                Session::Guard guard(session);
                if (guard.state.key == snapshot.key && guard.state.phase == Phase::Closing)
                    guard.state.peer_retired = true;
            }
            continue;
        }
        if (snapshot.phase != Phase::Open)
            continue;
        if (slot.route != snapshot.key) {
            Session::Guard guard(session);
            if (!guard.active(snapshot.key))
                continue;
            open_slot(slot, snapshot.key, now);
        }
        for (auto &pending : slot.exchanges)
            advance(slot, pending, now);
        auto &automatic = slot.exchanges[0];
        if (!automatic.active && automatic.outcome != Outcome::Succeeded) {
            const auto reason = automatic.outcome == Outcome::ResponseTimeout ? Failure::ResponseTimeout
                                : automatic.outcome == Outcome::RemoteNack    ? Failure::RemoteNack
                                : automatic.outcome == Outcome::SendDeadline  ? Failure::SendDeadline
                                : automatic.outcome == Outcome::TransportLost ? Failure::TransportLost
                                : automatic.outcome == Outcome::Rejected      ? Failure::IdentityConflict
                                                                              : Failure::LocalFull;
            session.close(slot.route, reason);
            continue;
        }
        if (!slot.established) {
            if (local_.role == icd::Role::MissionComputer && !automatic.active && !automatic.ticket)
                start_exchange(slot, automatic, icd::Type::HelloRequest, now);
            else if (local_.role == icd::Role::Node && now - snapshot.opened_at >= passive_hello_timeout_ms)
                session.close(slot.route, Failure::ResponseTimeout);
        } else if (!automatic.active && !automatic.ticket && now >= slot.next_heartbeat) {
            start_exchange(slot, automatic, icd::Type::KeepAliveRequest, now);
        }
    }
}
void PeerService::on_message(const runtime::Message &message, uint64_t) {
    const auto now = clock_.now();
    std::lock_guard<std::mutex> state(state_mutex_);
    if (message.id == probe_request_id) {
        ProbeRequest probe{};
        if (!message.read(probe))
            return;
        for (size_t i = 0; i < port_.count(); ++i) {
            auto &slot = slots_[i];
            if (!slot.established || peers_[slot.peer_index].descriptor.device != probe.device ||
                slot.exchanges[1].active || slot.exchanges[1].ticket || !port_.at(i).active(slot.route))
                continue;
            start_exchange(slot, slot.exchanges[1], icd::Type::KeepAliveRequest, now, &probe);
            return;
        }
        const ProbeResult reply{probe.device, 0, probe.console_generation, probe.request,
                                Outcome::LocalRejected};
        dispatcher_.deliver(runtime::destination(diagnostics_endpoint_),
                            runtime::Message::make(probe_result_id, probe.request, reply));
        return;
    }
    Inbound input{};
    if (!message.read(input))
        return;
    auto *session = port_.session(input.route);
    if (!session)
        return;
    auto &slot = slots_[input.route.slot];
    icd::Packet response{};
    bool reply = false, reject = false;
    PendingExchange *completed = nullptr;
    Outcome outcome = Outcome::Succeeded;
    {
        Session::Guard guard(*session);
        if (!guard.active(input.route)) {
            ++stale_;
            return;
        }
        if (slot.route != input.route)
            open_slot(slot, input.route, now);
        const auto type = icd::Type(message.id - incoming_base);
        if (type == icd::Type::HelloRequest) {
            const auto hello = input.body.get<icd::Hello>();
            const auto accepted = register_peer(slot, hello.device, now);
            response.type = icd::Type::HelloResponse;
            response.exchange = message.request;
            icd::Hello response_body{local_, now};
            reply = true;
            if (!accepted) {
                response_body.device.readiness = icd::Readiness::Rejected;
                response_body.device.reason = icd::Reason::IdentityConflict;
                reject = true;
                journal_.append(Event::IdentityConflict, now, hello.device.device, hello.device.boot,
                                "hello rejected", 2);
            }
            response.body.set(response_body);
        } else if (type == icd::Type::KeepAliveRequest) {
            if (!slot.established) {
                reject = true;
            } else {
                response.type = icd::Type::KeepAliveResponse;
                response.exchange = message.request;
                response.body.set(icd::Heartbeat{input.body.get<icd::Heartbeat>().origin_ms, now,
                                                 local_.reason, local_.readiness, 0});
                reply = true;
                peers_[slot.peer_index].last_seen = now;
            }
        } else {
            PendingExchange *match = nullptr;
            for (auto &p : slot.exchanges) {
                if (!p.active || p.request.exchange != message.request)
                    continue;
                const bool matching =
                    (type == icd::Type::HelloResponse && p.request.type == icd::Type::HelloRequest) ||
                    (type == icd::Type::KeepAliveResponse && p.request.type == icd::Type::KeepAliveRequest) ||
                    (type == icd::Type::Receipt &&
                     input.body.get<icd::Receipt>().request_type == p.request.type);
                if (matching)
                    match = &p;
            }
            if (!match || (match->has_written && now >= match->deadline)) {
                ++stale_;
                return;
            }
            if (type == icd::Type::Receipt) {
                if (input.body.get<icd::Receipt>().admission != icd::Admission::Accepted) {
                    completed = match;
                    outcome = Outcome::RemoteNack;
                }
            } else if (type == icd::Type::HelloResponse) {
                completed = match;
                const auto hello = input.body.get<icd::Hello>();
                if (!register_peer(slot, hello.device, now)) {
                    outcome = Outcome::Rejected;
                    reject = true;
                    journal_.append(Event::IdentityConflict, now, hello.device.device, hello.device.boot,
                                    "hello response rejected", 2);
                }
            } else if (type == icd::Type::KeepAliveResponse && slot.established) {
                const auto heartbeat = input.body.get<icd::Heartbeat>();
                if (heartbeat.origin_ms != match->request.body.get<icd::Heartbeat>().origin_ms) {
                    ++stale_;
                    return;
                }
                readiness(slot, heartbeat.readiness, heartbeat.reason, now);
                completed = match;
            }
            if (completed) {
                // Commit while the generation guard is held. Completion side effects
                // (ticket cancellation / dispatch) are performed after releasing it.
                completed->outcome = outcome;
                slot.next_heartbeat = now + heartbeat_interval_ms;
            }
        }
    }
    if (completed)
        complete(slot, *completed, outcome, now);
    if (reply && port_.send(input.route, response) != Post::Accepted)
        session->close(input.route, Failure::LocalFull);
    // A rejected Hello must be able to leave the TX inbox. The initiator closes
    // after observing the semantic rejection; an unestablished node also has
    // its bounded handshake deadline. Out-of-phase messages have no reply.
    if (reject && !reply)
        session->close(input.route, Failure::IdentityConflict);
}
} // namespace raven::services
