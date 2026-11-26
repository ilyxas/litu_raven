#pragma once
#include "raven_comms/tx_component.hpp"
#include "raven_core/service.hpp"
#include "raven_diagnostics/journal.hpp"
#include "raven_services/console_failure.hpp"
#include "raven_services/peer_state.hpp"
#include <array>
#include <atomic>
namespace raven::services {
constexpr uint16_t console_command_id = 0x400, console_line_id = 0x401, console_watch_id = 0x402;
enum class ConsoleCommand : uint8_t {
    Invalid,
    Help,
    Status,
    Peers,
    Peer,
    Ping,
    Stats,
    Logs,
    WatchOn,
    WatchOff
};
struct ConsoleRequest {
    uint64_t argument{};
    uint32_t generation{}, request{};
    ConsoleCommand command{};
};
struct ConsoleLine {
    uint32_t generation{};
    uint8_t size{};
    char text[56]{};
};
struct ConsoleWatch {
    uint32_t generation{};
    bool enabled{};
};
static_assert(sizeof(ConsoleLine) <= runtime::Message::payload_capacity);
struct Metrics {
    uint32_t free_heap{}, minimum_heap{}, largest_block{}, minimum_stack{}, sessions{}, stale{},
        diagnostic_drops{};
};
struct MetricsSource {
    Metrics (*read)(void *){};
    void *context{};
};
struct EventOutput {
    void (*write)(void *, const diagnostics::Record &){};
    void *context{};
};
// Reusable line parser. Reconnect resets partial input; an oversized or invalid
// line is discarded through LF and cannot become a truncated valid command.
class ConsoleParser {
  public:
    using Sink = void (*)(void *, const ConsoleRequest &);
    void feed(uint32_t generation, const uint8_t *bytes, size_t size, Sink sink, void *context);

  private:
    ConsoleRequest parse();
    std::array<char, 128> line_{};
    size_t used_{};
    uint32_t generation_{}, request_{};
    bool overflow_{};
};
// Console queries and event publication run below peer execution priority.
// Long replies advance one line per tick instead of filling TX with a dump.
class DiagnosticsService final : public runtime::Service {
  public:
    DiagnosticsService(comms::CommunicationPort &port, const PeerState &peers,
                       runtime::Dispatcher &dispatcher, diagnostics::Journal &journal, comms::Clock clock,
                       MetricsSource metrics, EventOutput output, uint8_t peer_endpoint, uint8_t ble_endpoint,
                       bool mission_computer, ConsoleFailure console_failure);
    void on_message(const runtime::Message &message, uint64_t now) override;
    void on_tick(uint64_t now) override;
    uint32_t drops() const { return drops_.load(); }

  private:
    bool line(uint32_t generation, const char *format, ...);
    void next_line();
    comms::CommunicationPort &port_;
    const PeerState &peers_;
    runtime::Dispatcher &dispatcher_;
    diagnostics::Journal &journal_;
    comms::Clock clock_;
    MetricsSource metrics_;
    EventOutput output_;
    uint8_t peer_endpoint_, ble_endpoint_;
    bool mission_computer_;
    ConsoleFailure console_failure_;
    ConsoleRequest reply_{};
    diagnostics::Record log_reply_{};
    PeerRecord peer_reply_{};
    uint8_t page_{};
    uint64_t cursor_{}, end_{}, output_cursor_{1}, network_cursor_{1}, next_network_{};
    std::atomic<uint32_t> drops_{0};
};
} // namespace raven::services
