#pragma once

#include <cstdint>

namespace raven {

// Initial read-only Betaflight command set. All commands fit MSP v1 framing.
enum class MspCommand : uint8_t {
    ApiVersion = 1,
    FlightControllerVariant = 2,
    FlightControllerVersion = 3,
    BoardInfo = 4,
    Status = 101,
    Rc = 105,
    RawGps = 106,
    Attitude = 108,
    Analog = 110,
    BoxIds = 119,
    BatteryState = 130,
};

} // namespace raven
