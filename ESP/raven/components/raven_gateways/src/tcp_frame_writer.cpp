#include "raven_gateways/tcp_frame_writer.hpp"
#include "raven_gateways/command_protocol.hpp"
#include "raven_platform/tcp_io.hpp"

namespace raven::demo {
bool send_frame(int fd, uint16_t id, const uint8_t* payload, size_t size) {
    if (size > 1024) return false;
    uint8_t frame[1030]{static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8)};
    wire::put32(frame + 2, static_cast<uint32_t>(size));
    std::memcpy(frame + 6, payload, size);
    // One frame/write avoids the Nagle + delayed-ACK interaction between a tiny
    // header and its payload. Partial writes are still completed by send_all.
    return tcp::send_all(fd, frame, size + 6);
}
} // namespace raven::demo
