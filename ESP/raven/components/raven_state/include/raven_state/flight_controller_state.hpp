#pragma once

#include <array>
#include <cstdint>
#include <mutex>

namespace raven {

enum class FlightControllerLinkStatus : uint8_t {
    Disconnected,
    Connected,
};

struct FlightControllerIdentity {
    uint8_t protocol_version{0};
    uint8_t api_major{0};
    uint8_t api_minor{0};
    std::array<char, 5> variant{};
    std::array<uint8_t, 3> firmware_version{};
};

struct FlightControllerSnapshot {
    FlightControllerLinkStatus link_status{FlightControllerLinkStatus::Disconnected};
    FlightControllerIdentity identity{};
    uint32_t last_success_ms{0};
    uint32_t last_failure_ms{0};
    uint32_t successful_transactions{0};
    uint32_t failed_transactions{0};
    uint32_t consecutive_failures{0};
    uint32_t generation{0};
};

// Canonical, task-safe state for the physical flight-controller link.
// It owns no task, performs no I/O, and publishes no events.
class FlightControllerState {
public:
    FlightControllerSnapshot snapshot() const;

    void mark_connected(const FlightControllerIdentity& identity, uint32_t now_ms);
    void record_success(uint32_t now_ms);
    void record_failure(uint32_t now_ms);
    void mark_disconnected();

private:
    mutable std::mutex mutex_;
    FlightControllerSnapshot snapshot_{};
};

} // namespace raven
