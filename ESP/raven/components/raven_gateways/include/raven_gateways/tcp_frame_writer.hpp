#pragma once
#include <cstdint>
#include <cstddef>

namespace raven::demo {
// Writes the existing iOS binary envelope as one buffer; completes partial sends.
bool send_frame(int fd, uint16_t id, const uint8_t* payload, size_t size);
} // namespace raven::demo
