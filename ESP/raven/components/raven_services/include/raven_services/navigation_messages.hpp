#pragma once

#include <cstdint>

namespace raven {
namespace nav_msg {

// Message kind shared by navigation directed messages and self-messages.
static constexpr uint16_t KIND = 0x0010;

// Message IDs within the navigation message kind.
static constexpr uint16_t MOVE_FORWARD = 0x0001;  // directed to NavigationService: execute forward move
static constexpr uint16_t MOVE_DONE    = 0x0002;  // self-message to NavigationActivity: service completed

} // namespace nav_msg
} // namespace raven
