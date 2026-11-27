#include "raven_comms/session.hpp"
#include <algorithm>
#include <cassert>
namespace raven::comms {
void Session::configure(uint32_t slot, uint8_t endpoint) {
    Guard g(*this);
    assert(g.state.phase == Phase::Vacant && !g.state.key.generation);
    g.state.key.slot = slot;
    g.state.tx_endpoint = endpoint;
}
SessionKey Session::open(uint64_t now) {
    Guard g(*this);
    if (g.state.phase != Phase::Vacant)
        return {};
    if (g.state.key.generation == UINT32_MAX) {
        g.state.phase = Phase::Exhausted;
        return {};
    }
    ++g.state.key.generation;
    g.state.phase = Phase::Open;
    g.state.failure = Failure::None;
    g.state.peer_retired = false;
    g.state.opened_at = now;
    g.state.diagnostic_pending = false;
    for (auto &s : g.state.sends)
        s.phase = SendPhase::Free;
    return g.state.key;
}
void Session::close(SessionKey key, Failure reason) {
    Guard g(*this);
    if (!g.active(key))
        return;
    g.state.phase = Phase::Closing;
    g.state.failure = reason;
    for (auto &s : g.state.sends) {
        if (s.phase == SendPhase::Queued || s.phase == SendPhase::Writing) {
            s.phase = SendPhase::Failed;
            s.reason = Failure::TransportLost;
        }
    }
}
Session::State Session::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}
bool Session::active(SessionKey key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_.phase == Phase::Open && state_.key == key;
}
bool Session::acquire_io(SessionKey key) {
    Guard g(*this);
    if (!g.active(key))
        return false;
    ++g.state.io_users;
    return true;
}
void Session::release_io() {
    Guard g(*this);
    assert(g.state.io_users);
    --g.state.io_users;
}
bool Session::retire() {
    Guard g(*this);
    if (g.state.phase != Phase::Closing || !g.state.peer_retired || g.state.io_users)
        return false;
    g.state.phase = Phase::Vacant;
    return true;
}
SendToken Session::reserve(SessionKey key, uint32_t exchange, icd::Type type, uint64_t now) {
    Guard g(*this);
    if (!g.active(key))
        return {};
    for (uint32_t i = 0; i < g.state.sends.size(); ++i) {
        auto &s = g.state.sends[i];
        if (s.phase != SendPhase::Free || s.generation == UINT32_MAX)
            continue;
        const auto generation = s.generation + 1;
        s = {now, 0, 0, key, exchange, generation, type, SendPhase::Queued, Failure::None};
        return {i, generation};
    }
    return {};
}
SendRecord Session::progress(SendToken token) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (token.slot < state_.sends.size() && state_.sends[token.slot].generation == token.generation)
        return state_.sends[token.slot];
    SendRecord invalid{};
    invalid.phase = SendPhase::Failed;
    invalid.reason = Failure::TransportLost;
    return invalid;
}
bool Session::begin(SendToken token, SessionKey key, uint64_t now, uint64_t &deadline) {
    Guard g(*this);
    if (!g.active(key))
        return false;
    deadline = now + write_budget_ms;
    if (!token)
        return true;
    if (token.slot >= g.state.sends.size())
        return false;
    auto &s = g.state.sends[token.slot];
    if (s.generation != token.generation || s.route != key || s.phase != SendPhase::Queued)
        return false;
    if (now >= s.admitted + local_send_budget_ms) {
        s.phase = SendPhase::Failed;
        s.reason = Failure::SendDeadline;
        s.finished = now;
        return false;
    }
    deadline = std::min(deadline, s.admitted + local_send_budget_ms);
    s.phase = SendPhase::Writing;
    s.started = now;
    return true;
}
void Session::finish(SendToken token, SessionKey key, uint64_t now, Failure failure) {
    if (!token)
        return;
    Guard g(*this);
    if (!g.active(key) || token.slot >= g.state.sends.size())
        return;
    auto &s = g.state.sends[token.slot];
    if (s.generation != token.generation || s.route != key || s.phase != SendPhase::Writing)
        return;
    if (now >= s.admitted + local_send_budget_ms && failure == Failure::None)
        failure = Failure::SendDeadline;
    s.finished = now;
    s.reason = failure;
    s.phase = failure == Failure::None ? SendPhase::Written : SendPhase::Failed;
}
void Session::cancel(SendToken token) {
    Guard g(*this);
    if (token.slot >= g.state.sends.size())
        return;
    auto &s = g.state.sends[token.slot];
    // A partially written frame must finish or close; cancellation only removes
    // an attempt that has not started writing to this stream.
    if (s.generation == token.generation && s.phase == SendPhase::Queued)
        s.phase = SendPhase::Cancelled;
}
void Session::release(SendToken token) {
    Guard g(*this);
    if (token.slot >= g.state.sends.size())
        return;
    auto &s = g.state.sends[token.slot];
    if (s.generation == token.generation && terminal(s.phase))
        s.phase = SendPhase::Free;
}
void Session::finish_diagnostic(SessionKey key) {
    Guard g(*this);
    if (g.active(key))
        g.state.diagnostic_pending = false;
}
namespace {
Post result(const runtime::DeliveryReport &report) {
    if (report.complete())
        return Post::Accepted;
    return report.reason == runtime::Delivery::Full ? Post::Full : Post::Invalid;
}
} // namespace
Post CommunicationPort::send(SessionKey route, const icd::Packet &packet, SendToken ticket) {
    auto *s = session(route);
    if (!s || !icd::known(packet.type))
        return Post::Invalid;
    Session::Guard g(*s);
    if (!g.active(route))
        return Post::StaleRoute;
    const bool diagnostic = packet.type == icd::Type::Diagnostic;
    if (diagnostic && g.state.diagnostic_pending)
        return Post::Full;
    const auto posted =
        result(dispatcher_.deliver(runtime::destination(g.state.tx_endpoint),
                                   runtime::Message::make(outgoing(packet.type), packet.exchange,
                                                          Outbound{route, ticket, packet.body})));
    if (diagnostic && posted == Post::Accepted)
        g.state.diagnostic_pending = true;
    return posted;
}
Post CommunicationPort::receive(SessionKey route, const icd::Packet &packet, uint64_t now) {
    auto *s = session(route);
    if (!s || !icd::known(packet.type))
        return Post::Invalid;
    Post admitted;
    {
        Session::Guard g(*s);
        if (!g.active(route))
            return Post::StaleRoute;
        const auto endpoint = packet.type == icd::Type::Diagnostic ? diagnostic_endpoint_ : peer_endpoint_;
        admitted = result(dispatcher_.deliver(runtime::destination(endpoint),
                                              runtime::Message::make(incoming(packet.type), packet.exchange,
                                                                     Inbound{route, now, packet.body})));
    }
    if (packet.requires_ack) {
        icd::Packet receipt{};
        receipt.type = icd::Type::Receipt;
        receipt.exchange = packet.exchange;
        receipt.body.set(icd::Receipt{packet.type,
                                      admitted == Post::Accepted ? icd::Admission::Accepted
                                      : admitted == Post::Full   ? icd::Admission::Full
                                                                 : icd::Admission::Invalid,
                                      0});
        if (send(route, receipt) != Post::Accepted)
            s->close(route, Failure::LocalFull);
    } else if (admitted != Post::Accepted && packet.type != icd::Type::Diagnostic) {
        s->close(route, Failure::LocalFull);
    }
    return admitted;
}
} // namespace raven::comms
