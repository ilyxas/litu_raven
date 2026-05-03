#pragma once

#include <cstdint>
#include <cstddef>

namespace raven {

#pragma pack(push, 1)
struct NetworkHeader {
    uint16_t msg_id;
    uint32_t payload_size;
};
#pragma pack(pop)

static_assert(sizeof(NetworkHeader) == 6, "Unexpected NetworkHeader size");

} // namespace raven