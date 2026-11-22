#pragma once
#include "raven_gateways/command_protocol.hpp"

namespace raven::demo {
// One parser per connection; supports fragmented headers/payloads and multiple
// frames in a read. An oversized length closes the connection before allocation.
class FrameReader {
public:
    using Emit = bool (*)(void*, const Response&);
    explicit FrameReader(Protocol& protocol) : protocol_(protocol) {}
    bool feed(const uint8_t* data, size_t size, Emit emit, void* context);
    // Limit a socket read to one header/payload phase, allowing the listener to
    // be serviced between frames even when the current client floods requests.
    size_t bytes_needed() const { return expected_ - used_; }
    void reset() { used_ = 0; expected_ = 6; }
private:
    Protocol& protocol_;
    std::array<uint8_t, 6 + wire::max_payload> bytes_{};
    size_t used_{}, expected_{6};
};
} // namespace raven::demo
