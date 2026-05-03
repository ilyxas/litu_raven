#pragma once

#include <cstdint>

// telemetry_messages.hpp — message IDs, kind constants, and wire types for
// the telemetry domain.
//
// TELEMETRY_DATA   (0x3001): outbound — JSON telemetry snapshot sent to the
//                             remote server via NetworkClientGateway.
// TELEMETRY_ACTIVE (0x3002): inbound  — enables or disables TelemetryService
//                             data collection and transmission.

namespace raven::net_msg {
    constexpr uint16_t TELEMETRY_DATA   = 0x3001;  // outbound telemetry snapshot
    constexpr uint16_t TELEMETRY_ACTIVE = 0x3002;  // inbound activation toggle
} // namespace raven::net_msg

namespace raven::msg_kind {
    constexpr uint16_t TELEMETRY = 0x0400;
} // namespace raven::msg_kind

namespace raven {

// Outbound telemetry snapshot passed to TelemetryJsonEncoder via msg.data.
// All three categories are captured at transmit time so the encoder can
// serialise the full snapshot without touching TelemetryState directly.
struct TelemetrySnapshot {
    uint32_t sent_ts;  // tick count at the moment of transmission

    struct Navigation {
        float    speed;
        float    direction;
        float    position_x;
        float    position_y;
        uint32_t ts;
    } navigation;

    struct System {
        int32_t  rssi;
        uint32_t heap_free;
        float    battery_pct;
        uint32_t ts;
    } system;

    struct Sensors {
        float    lidar_distance;
        float    humidity;
        float    temperature;
        float    noise_level;
        uint32_t ts;
    } sensors;
};

// Inbound payload for TELEMETRY_ACTIVE messages.
// active == 1 → start collecting and transmitting telemetry.
// active == 0 → stop.
struct TelemetryActivePayload {
    uint8_t active;
};

} // namespace raven
