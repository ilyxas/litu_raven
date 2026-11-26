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
void observe_written(PendingExchange &pending, const SendRecord &progress) {
    if (pending.active && !pending.has_written && progress.phase == SendPhase::Written) {
        pending.has_written = true;
        pending.first_written = progress.finished;
        pending.deadline = progress.finished + semantic_timeout_ms;
        pending.next_retry = progress.finished + hello_retry_ms;
    }
}
} // namespace
PeerService::PeerService(CommunicationPort &port, runtime::Dispatcher &dispatcher, PeerState &state,
                         Clock clock, PeerSlot *slots, diagnostics::Journal &journal, uint8_t endpoint,
                         ConsoleFailure console_failure)
    : port_(port), dispatcher_(dispatcher), state_(state), clock_(clock), slots_(slots), journal_(journal),
      diagnostics_endpoint_(endpoint), console_failure_(console_failure) {
    assert(console_failure_.close);
    for (uint16_t id = uint16_t(icd::Type::HelloRequest); id <= uint16_t(icd::Type::Receipt); ++id) {
        const bool ok = inbox.subscribe(incoming(icd::Type(id)), sizeof(Inbound), runtime::Policy::Ordered);
        assert(ok);
        (void)ok;
    }
    const bool ok = inbox.subscribe(probe_request_id, sizeof(ProbeRequest), runtime::Policy::Ordered);
    assert(ok);
    (void)ok;
}
bool PeerService::register_peer(PeerState::Write &state, PeerSlot &slot,
                                const icd::DeviceDescriptor &descriptor, uint64_t now) {
    if (!descriptor.device || descriptor.device == state_.identity().device ||
        descriptor.protocol != icd::version || descriptor.readiness == icd::Readiness::Rejected)
        return false;
    // Identity of an established transport binding is immutable.
    if (slot.established) {
        const auto &old = state.record(slot.peer_index).descriptor;
        return old.device == descriptor.device && old.boot == descriptor.boot;
    }
    size_t selected = SIZE_MAX;
    for (size_t i = 0; i < state.capacity(); ++i) {
        if (state.record(i).descriptor.device != descriptor.device)
            continue;
        if (state.record(i).status != PeerStatus::Disconnected && state.record(i).route != slot.route)
            return false;
        selected = i;
        break;
    }
    if (selected == SIZE_MAX) {
        for (size_t i = 0; i < state.capacity(); ++i) {
            if (!state.record(i).descriptor.device) {
                selected = i;
                break;
            }
            if (state.record(i).status == PeerStatus::Disconnected &&
                (selected == SIZE_MAX || state.record(i).changed_at < state.record(selected).changed_at))
                selected = i;
        }
    }
    if (selected == SIZE_MAX)
        return false;
    state.record(selected) = {descriptor, slot.route, now, now, PeerStatus::Handshaking, Failure::None};
    slot.peer_index = selected;
    slot.established = true;
    slot.next_heartbeat = now + heartbeat_interval_ms;
    readiness(state, slot, descriptor.readiness, descriptor.reason, now);
    return true;
}
void PeerService::readiness(PeerState::Write &state, PeerSlot &slot, icd::Readiness ready, icd::Reason reason,
                            uint64_t now) {
    if (!slot.established)
        return;
    auto &peer = state.record(slot.peer_index);
    const auto status = ready == icd::Readiness::Ready ? PeerStatus::Ready : PeerStatus::NotReady;
    peer.last_seen = now;
    peer.descriptor.readiness = ready;
    peer.descriptor.reason = reason;
    if (status == peer.status)
        return;
    peer.status = status;
    peer.changed_at = now;
    journal_.append(status == PeerStatus::Ready ? Event::PeerReady : Event::PeerNotReady, now,
                    peer.descriptor.device, peer.descriptor.boot, name(status));
}
void PeerService::publish_probe(const ProbeResult &result, uint64_t now, icd::BootId boot) {
    const auto delivered =
        dispatcher_.deliver(runtime::destination(diagnostics_endpoint_),
                            runtime::Message::make(probe_result_id, result.request, result));
    // A finite operation retains its own outcome. Failure to deliver its
    // mandatory reply invalidates only the originating console generation.
    if (!delivered.complete())
        console_failure_(result.console_generation);
    journal_.append(Event::ProbeCompleted, now, result.device, boot,
                    delivered.complete() ? name(result.outcome) : "probe complete; console delivery failed");
}
void PeerService::complete(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, Outcome result,
                           uint64_t now) {
    if (!pending.active)
        return;
    auto *session = port_.session(slot.route);
    if (session && pending.ticket)
        observe_written(pending, session->progress(pending.ticket));
    pending.active = false;
    pending.outcome = result;
    if (session && pending.ticket)
        session->cancel(pending.ticket);
    const auto id = slot.established ? state.record(slot.peer_index).descriptor.device : 0;
    const auto boot = slot.established ? state.record(slot.peer_index).descriptor.boot : 0;
    if (pending.manual) {
        const bool measured =
            result == Outcome::Succeeded && pending.has_written && now >= pending.first_written;
        const ProbeResult reply{id,
                                measured ? now - pending.first_written : 0,
                                pending.console_generation,
                                pending.console_request,
                                result,
                                measured};
        publish_probe(reply, now, boot);
    } else if (result != Outcome::Succeeded) {
        journal_.append(Event::ExchangeFailed, now, id, boot, name(result), 2);
    }
}
void PeerService::attempt(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, uint64_t now) {
    auto &session = port_.at(slot.route.slot);
    const auto ticket = session.reserve(slot.route, pending.request.exchange, pending.request.type, now);
    if (!ticket) {
        complete(state, slot, pending, Outcome::LocalRejected, now);
        return;
    }
    pending.ticket = ticket;
    ++pending.attempts;
    const auto posted = port_.send(slot.route, pending.request, ticket);
    if (posted != Post::Accepted) {
        session.cancel(ticket);
        session.release(ticket);
        pending.ticket = {};
        complete(state, slot, pending,
                 posted == Post::StaleRoute ? Outcome::TransportLost : Outcome::LocalRejected, now);
    }
}
void PeerService::start_exchange(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending,
                                 icd::Type type, uint64_t now, const ProbeRequest *probe) {
    if (pending.active || pending.ticket)
        return;
    if (exchange_counter_ == UINT32_MAX) {
        if (probe)
            publish_probe(
                {probe->device, 0, probe->console_generation, probe->request, Outcome::LocalRejected}, now);
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
        pending.request.body.set(icd::Hello{state_.identity(), now});
    else
        pending.request.body.set(
            icd::Heartbeat{now, now, state_.identity().reason, state_.identity().readiness, 0});
    if (probe) {
        pending.manual = true;
        pending.console_generation = probe->console_generation;
        pending.console_request = probe->request;
    }
    attempt(state, slot, pending, now);
}
void PeerService::advance(PeerState::Write &state, PeerSlot &slot, PendingExchange &p, uint64_t now) {
    auto &session = port_.at(slot.route.slot);
    if (p.ticket) {
        const auto progress = session.progress(p.ticket);
        observe_written(p, progress);
        if (progress.phase == SendPhase::Failed && p.active)
            complete(state, slot, p,
                     progress.reason == Failure::SendDeadline ? Outcome::SendDeadline
                                                              : Outcome::TransportLost,
                     now);
        if (terminal(progress.phase)) {
            session.release(p.ticket);
            p.ticket = {};
        } else if (p.active && now >= progress.admitted + Session::local_send_budget_ms) {
            complete(state, slot, p, Outcome::SendDeadline, now);
            session.close(slot.route, Failure::SendDeadline);
        }
    }
    if (!p.active)
        return;
    if (p.has_written && now >= p.deadline) {
        complete(state, slot, p, Outcome::ResponseTimeout, now);
        return;
    }
    if (p.has_written && p.request.type == icd::Type::HelloRequest && !p.ticket &&
        p.attempts < hello_attempt_limit && now >= p.next_retry) {
        p.next_retry = p.first_written + uint64_t(p.attempts + 1) * hello_retry_ms;
        attempt(state, slot, p, now);
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
void PeerService::close_slot(PeerState::Write &state, PeerSlot &slot, Failure failure, uint64_t now) {
    for (auto &pending : slot.exchanges) {
        // Session closure fails other queued writes with TransportLost, while
        // preserving an already terminal ticket. Retire each exchange using
        // its own send result, independently of which owner ran first.
        const auto progress = port_.at(slot.route.slot).progress(pending.ticket);
        const auto outcome = pending.ticket && progress.route == slot.route &&
                                     progress.phase == SendPhase::Failed &&
                                     progress.reason == Failure::SendDeadline
                                 ? Outcome::SendDeadline
                                 : Outcome::TransportLost;
        complete(state, slot, pending, outcome, now);
    }
    icd::DeviceId id = 0;
    icd::BootId boot = 0;
    if (slot.established) {
        auto &peer = state.record(slot.peer_index);
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
    PeerState::Write state(state_);
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
                close_slot(state, slot, snapshot.failure, now);
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
            advance(state, slot, pending, now);
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
            if (state_.identity().role == icd::Role::MissionComputer && !automatic.active &&
                !automatic.ticket)
                start_exchange(state, slot, automatic, icd::Type::HelloRequest, now);
            else if (state_.identity().role == icd::Role::Node &&
                     now - snapshot.opened_at >= passive_hello_timeout_ms)
                session.close(slot.route, Failure::ResponseTimeout);
        } else if (!automatic.active && !automatic.ticket && now >= slot.next_heartbeat) {
            start_exchange(state, slot, automatic, icd::Type::KeepAliveRequest, now);
        }
    }
}
void PeerService::on_message(const runtime::Message &message, uint64_t) {
    const auto now = clock_.now();
    PeerState::Write state(state_);
    if (message.id == probe_request_id) {
        ProbeRequest probe{};
        if (!message.read(probe))
            return;
        for (size_t i = 0; i < port_.count(); ++i) {
            auto &slot = slots_[i];
            if (!slot.established || state.record(slot.peer_index).descriptor.device != probe.device ||
                slot.exchanges[1].active || slot.exchanges[1].ticket || !port_.at(i).active(slot.route))
                continue;
            start_exchange(state, slot, slot.exchanges[1], icd::Type::KeepAliveRequest, now, &probe);
            return;
        }
        const ProbeResult reply{probe.device, 0, probe.console_generation, probe.request,
                                Outcome::LocalRejected};
        publish_probe(reply, now);
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
            state.note_stale();
            return;
        }
        if (slot.route != input.route)
            open_slot(slot, input.route, now);
        const auto type = icd::Type(message.id - incoming_base);
        if (type == icd::Type::HelloRequest) {
            const auto hello = input.body.get<icd::Hello>();
            const auto accepted = register_peer(state, slot, hello.device, now);
            response.type = icd::Type::HelloResponse;
            response.exchange = message.request;
            icd::Hello response_body{state_.identity(), now};
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
                                                 state_.identity().reason, state_.identity().readiness, 0});
                reply = true;
                state.record(slot.peer_index).last_seen = now;
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
            // A response may be handled before the next tick. Observe Written
            // under the existing generation guard before checking its deadline.
            if (match && match->ticket && match->ticket.slot < guard.state.sends.size()) {
                const auto &progress = guard.state.sends[match->ticket.slot];
                if (progress.generation == match->ticket.generation && progress.route == input.route)
                    observe_written(*match, progress);
            }
            if (!match || (match->has_written && now >= match->deadline)) {
                state.note_stale();
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
                if (!register_peer(state, slot, hello.device, now)) {
                    outcome = Outcome::Rejected;
                    reject = true;
                    journal_.append(Event::IdentityConflict, now, hello.device.device, hello.device.boot,
                                    "hello response rejected", 2);
                }
            } else if (type == icd::Type::KeepAliveResponse && slot.established) {
                const auto heartbeat = input.body.get<icd::Heartbeat>();
                if (heartbeat.origin_ms != match->request.body.get<icd::Heartbeat>().origin_ms) {
                    state.note_stale();
                    return;
                }
                readiness(state, slot, heartbeat.readiness, heartbeat.reason, now);
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
        complete(state, slot, *completed, outcome, now);
    if (reply && port_.send(input.route, response) != Post::Accepted)
        session->close(input.route, Failure::LocalFull);
    // A rejected Hello must be able to leave the TX inbox. The initiator closes
    // after observing the semantic rejection; an unestablished node also has
    // its bounded handshake deadline. Out-of-phase messages have no reply.
    if (reject && !reply)
        session->close(input.route, Failure::IdentityConflict);
}
} // namespace raven::services
