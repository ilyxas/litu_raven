#pragma once
#include "raven_icd/device_protocol.hpp"

namespace raven::comms {
// Stateful length framing belongs to the channel boundary, not to the codec.
// A packet view is valid only during Sink; adapters must copy admitted values.
class LengthFrameReader {
  public:
    using Sink = bool (*)(void *, const uint8_t *, size_t, uint64_t);
    bool feed(const uint8_t *bytes, size_t size, uint64_t now, Sink sink, void *context);
    bool expired(uint64_t now) const { return used_ && now - started_ >= 1000; }
    void reset() {
        used_ = expected_ = 0;
        started_ = 0;
        failed_ = false;
    }

  private:
    std::array<uint8_t, icd::max_packet + 2> buffer_{};
    size_t used_{}, expected_{};
    uint64_t started_{};
    bool failed_{};
};
// Adds the TCP profile's length prefix to an already encoded packet. A future
// datagram/UART adapter chooses its own framing at the PacketWriter boundary.
size_t length_frame(const uint8_t *packet, size_t size, uint8_t *out, size_t capacity);
} // namespace raven::comms
