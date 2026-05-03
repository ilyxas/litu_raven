#pragma once

#include <cstdint>

// pilot_messages.hpp — message IDs, kind constants, and payload structs for
// the current navigation/pilot example flow.
//
// These definitions describe what the navigation domain receivers
// (NavigationService, NavigationActivity) are prepared to accept from the
// network gateway. They are adjacent to the service domain rather than
// buried in raven_core.

namespace raven::net_msg {
    constexpr uint16_t JOYSTICK_INPUT        = 0x1001;
    constexpr uint16_t START_MANUAL_NAV      = 0x1002;
    constexpr uint16_t HALT_MANUAL_NAV       = 0x1003;
    constexpr uint16_t LLM_RESPONSE_TEXT     = 0x2001;
} // namespace raven::net_msg

namespace raven::msg_kind {
    constexpr uint16_t PILOT_INPUT = 0x0100;
    constexpr uint16_t NAV_CMD     = 0x0200;
    constexpr uint16_t LLM_DATA    = 0x0300;
} // namespace raven::msg_kind

namespace raven {

struct JoystickPayload {
    float    x;
    float    y;
    uint32_t timestamp_ms;
};

struct StartManualPayload {
    uint32_t timestamp_ms;
};

struct HaltManualPayload {
    uint32_t timestamp_ms;
};

} // namespace raven
