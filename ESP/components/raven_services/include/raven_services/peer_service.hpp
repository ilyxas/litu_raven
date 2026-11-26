#pragma once
#include "raven_comms/tx_component.hpp"
#include "raven_core/service.hpp"
#include "raven_diagnostics/journal.hpp"
#include "raven_services/console_failure.hpp"
#include "raven_services/peer_state.hpp"

namespace raven::services {
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

// Owns peer operations and exchange deadlines; publishes shared knowledge to
// PeerState. Composition supplies storage/capacity, with no board-specific paths.
// Live binding commits hold both PeerState::Write and Session::Guard. Sockets,
// TCP-segment retransmission and TX buffers remain outside this behavior owner.
class PeerService final : public runtime::Service {
  public:
    PeerService(comms::CommunicationPort &port, runtime::Dispatcher &dispatcher, PeerState &state,
                comms::Clock clock, PeerSlot *slots, diagnostics::Journal &journal,
                uint8_t diagnostics_endpoint, ConsoleFailure console_failure);
    void on_message(const runtime::Message &message, uint64_t now) override;
    void on_tick(uint64_t now) override;

  private:
    void publish_probe(const ProbeResult &result, uint64_t now, icd::BootId boot = 0);
    void open_slot(PeerSlot &slot, comms::SessionKey route, uint64_t now);
    bool register_peer(PeerState::Write &state, PeerSlot &slot, const icd::DeviceDescriptor &descriptor,
                       uint64_t now);
    void readiness(PeerState::Write &state, PeerSlot &slot, icd::Readiness ready, icd::Reason reason,
                   uint64_t now);
    void start_exchange(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, icd::Type type,
                        uint64_t now, const ProbeRequest *probe = nullptr);
    void attempt(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, uint64_t now);
    void complete(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, Outcome result,
                  uint64_t now);
    void advance(PeerState::Write &state, PeerSlot &slot, PendingExchange &pending, uint64_t now);
    void close_slot(PeerState::Write &state, PeerSlot &slot, comms::Failure failure, uint64_t now);
    comms::CommunicationPort &port_;
    runtime::Dispatcher &dispatcher_;
    PeerState &state_;
    comms::Clock clock_;
    PeerSlot *slots_;
    diagnostics::Journal &journal_;
    uint8_t diagnostics_endpoint_;
    ConsoleFailure console_failure_;
    uint32_t exchange_counter_{};
};
} // namespace raven::services
