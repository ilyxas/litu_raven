#pragma once

#include <cstdint>
#include <vector>

namespace raven {

enum class MspDirection : uint8_t {
    ToFlightController,
    FromFlightController,
    Error,
};

struct MspFrame {
    MspDirection direction{MspDirection::FromFlightController};
    uint16_t command{0};
    std::vector<uint8_t> payload;
};

} // namespace raven
