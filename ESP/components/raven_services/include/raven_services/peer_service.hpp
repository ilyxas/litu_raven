#pragma once
#include "raven_comms/tx_component.hpp"
#include "raven_diagnostics/journal.hpp"
#include <mutex>

namespace raven::services {
enum class PeerStatus : uint8_t { Unknown, Handshaking, NotReady, Ready, Disconnected };
struct PeerRecord {
    icd::DeviceDescriptor descriptor{};
    comms::SessionKey route{};
    uint64_t last_seen{}, changed_at{};
    PeerStatus status{};
    comms::Failure failure{};
};
enum class Outcome : uint8_t {
    Pending,
    Succeeded,
    LocalRejected,
    TransportLost,
    SendDeadline,
    RemoteNack,
    ResponseTimeout,
    Rejected
};
struct PendingExchange {
    icd::Packet request{};
    comms::SendToken ticket{};
    uint64_t first_written{}, next_retry{}, deadline{};
    uint32_t console_generation{}, console_request{};
    Outcome outcome{Outcome::Succeeded};
    uint8_t attempts{};
    bool has_written{}, active{}, manual{};
};
struct PeerSlot {
    comms::SessionKey route{};
    uint64_t next_heartbeat{};
    size_t peer_index{SIZE_MAX};
    std::array<PendingExchange, 2> exchanges{};
    bool established{};
};
constexpr uint16_t probe_request_id = 0x300, probe_result_id = 0x301;
struct ProbeRequest {
    icd::DeviceId device{};
    uint32_t console_generation{}, request{};
};
struct ProbeResult {
    icd::DeviceId device{};
    uint64_t elapsed_ms{};
    uint32_t console_generation{}, request{};
    Outcome outcome{};
};

// Owns semantic peer state and exchange deadlines. Storage/capacity are supplied
// by composition, so adding a board does not add a peer-specific code path.
// All changes to a live binding hold both state_mutex_ and its Session::Guard.
// Sockets, retransmission of TCP segments and TX buffers are outside this owner.
class PeerService final : public runtime::Component {
  public:
    PeerService(comms::CommunicationPort &port, runtime::Dispatcher &dispatcher,
                const icd::DeviceDescriptor &local, comms::Clock clock, PeerSlot *slots, PeerRecord *peers,
                size_t peer_capacity, diagnostics::Journal &journal, uint8_t diagnostics_endpoint);
    void on_message(const runtime::Message &message, uint64_t now) override;
    void on_tick(uint64_t now) override;
    bool peer(size_t index, PeerRecord &value) const;
    size_t capacity() const { return peer_capacity_; }
    icd::DeviceDescriptor identity() const { return local_; }
    runtime::Delivery probe(icd::DeviceId id, uint32_t console_generation, uint32_t request);
    uint32_t stale_messages() const;

  private:
    void open_slot(PeerSlot &slot, comms::SessionKey route, uint64_t now);
    bool register_peer(PeerSlot &slot, const icd::DeviceDescriptor &descriptor, uint64_t now);
    void readiness(PeerSlot &slot, icd::Readiness ready, icd::Reason reason, uint64_t now);
    void start_exchange(PeerSlot &slot, PendingExchange &pending, icd::Type type, uint64_t now,
                        const ProbeRequest *probe = nullptr);
    void attempt(PeerSlot &slot, PendingExchange &pending, uint64_t now);
    void complete(PeerSlot &slot, PendingExchange &pending, Outcome result, uint64_t now);
    void advance(PeerSlot &slot, PendingExchange &pending, uint64_t now);
    void close_slot(PeerSlot &slot, comms::Failure failure, uint64_t now);
    comms::CommunicationPort &port_;
    runtime::Dispatcher &dispatcher_;
    icd::DeviceDescriptor local_;
    comms::Clock clock_;
    PeerSlot *slots_;
    PeerRecord *peers_;
    size_t peer_capacity_;
    diagnostics::Journal &journal_;
    uint8_t diagnostics_endpoint_;
    mutable std::mutex state_mutex_;
    uint32_t exchange_counter_{}, stale_{};
};
const char *name(PeerStatus status);
const char *name(Outcome result);
const char *name(comms::Failure failure);
} // namespace raven::services
