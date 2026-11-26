#pragma once
#include "raven_core/dispatcher.hpp"
#include "raven_icd/device_protocol.hpp"
#include <array>
#include <limits>
#include <mutex>

namespace raven::comms {
struct SessionKey {
    uint32_t slot{}, generation{};
    friend bool operator==(SessionKey a, SessionKey b) {
        return a.slot == b.slot && a.generation == b.generation;
    }
    friend bool operator!=(SessionKey a, SessionKey b) { return !(a == b); }
    explicit operator bool() const { return generation != 0; }
};
using ReplyRoute = SessionKey;
enum class Phase : uint8_t { Vacant, Open, Closing, Exhausted };
enum class Failure : uint8_t {
    None,
    TransportLost,
    Protocol,
    LocalFull,
    SendDeadline,
    WriteError,
    ResponseTimeout,
    RemoteNack,
    IdentityConflict
};
enum class SendPhase : uint8_t { Free, Queued, Writing, Written, Failed, Cancelled };
struct SendToken {
    uint32_t slot{UINT32_MAX}, generation{};
    explicit operator bool() const { return slot != UINT32_MAX; }
};
struct SendRecord {
    uint64_t admitted{}, started{}, finished{};
    SessionKey route{};
    uint32_t exchange{}, generation{};
    icd::Type type{};
    SendPhase phase{};
    Failure reason{};
};
static_assert(sizeof(SendRecord) == 48);
inline bool terminal(SendPhase phase) {
    return phase == SendPhase::Written || phase == SendPhase::Failed || phase == SendPhase::Cancelled;
}

// One reusable channel slot. The same lock linearizes close, admission and
// peer-state commits. No I/O or behavioral callback may run while holding it.
// Lock order: PeerService state -> Session -> Inbox. RX never takes PeerState.
class Session {
  public:
    static constexpr uint64_t local_send_budget_ms = 250;
    static constexpr uint64_t write_budget_ms = 100;
    struct State {
        SessionKey key{};
        Phase phase{Phase::Vacant};
        Failure failure{};
        uint8_t tx_endpoint{};
        bool peer_retired{true};
        bool diagnostic_pending{};
        uint32_t io_users{};
        uint64_t opened_at{};
        std::array<SendRecord, 2> sends{};
    };
    class Guard {
      public:
        explicit Guard(Session &session) : lock_(session.mutex_), state(session.state_) {}
        bool active(SessionKey key) const { return state.phase == Phase::Open && state.key == key; }

      private:
        std::unique_lock<std::mutex> lock_;

      public:
        State &state;
    };
    void configure(uint32_t slot, uint8_t endpoint);
    SessionKey open(uint64_t now);
    void close(SessionKey key, Failure reason);
    State snapshot() const;
    bool active(SessionKey key) const;
    bool acquire_io(SessionKey key);
    void release_io();
    // Platform calls retire only after RX reset, TX inbox drain and socket close.
    bool retire();
    SendToken reserve(SessionKey key, uint32_t exchange, icd::Type type, uint64_t now);
    SendRecord progress(SendToken token) const;
    bool begin(SendToken token, SessionKey key, uint64_t now, uint64_t &deadline);
    void finish(SendToken token, SessionKey key, uint64_t now, Failure failure);
    void cancel(SendToken token);
    void release(SendToken token);
    void finish_diagnostic(SessionKey key);

  private:
    mutable std::mutex mutex_;
    State state_{};
};

constexpr uint16_t incoming_base = 0x100, outgoing_base = 0x200;
constexpr uint16_t incoming(icd::Type type) {
    return incoming_base + uint16_t(type);
}
constexpr uint16_t outgoing(icd::Type type) {
    return outgoing_base + uint16_t(type);
}
struct Inbound {
    SessionKey route;
    uint64_t received_ms{};
    icd::Body body;
};
struct Outbound {
    SessionKey route;
    SendToken ticket;
    icd::Body body;
};
static_assert(sizeof(Inbound) == 64 && sizeof(Outbound) == 64);
enum class Post { Accepted, Full, Invalid, StaleRoute };

// Checked local dispatch across a communication boundary. It adds no queue and
// does not turn Dispatcher into a global/remote router. Session count is supplied
// by composition; a transport adapter translates ICD into local endpoint IDs.
class CommunicationPort {
  public:
    CommunicationPort(runtime::Dispatcher &dispatcher, Session *sessions, size_t count, uint8_t peer_endpoint,
                      uint8_t diagnostic_endpoint)
        : dispatcher_(dispatcher), sessions_(sessions), count_(count), peer_endpoint_(peer_endpoint),
          diagnostic_endpoint_(diagnostic_endpoint) {}
    Post send(SessionKey route, const icd::Packet &packet, SendToken ticket = {});
    Post receive(SessionKey route, const icd::Packet &packet, uint64_t now);
    Session *session(SessionKey route) const {
        return route.slot < count_ ? &sessions_[route.slot] : nullptr;
    }
    Session &at(size_t slot) const { return sessions_[slot]; }
    size_t count() const { return count_; }

  private:
    runtime::Dispatcher &dispatcher_;
    Session *sessions_;
    size_t count_;
    uint8_t peer_endpoint_, diagnostic_endpoint_;
};
} // namespace raven::comms
