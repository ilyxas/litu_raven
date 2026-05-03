#pragma once

#include "raven_core/task_message.hpp"
#include "raven_gateways/network_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace raven {

// EncodedFrame — non-owning view of a complete outbound Raven wire frame.
//
// `data` points to a contiguous buffer containing the NetworkHeader followed
// immediately by the payload bytes.  The buffer is owned by the encoder that
// produced it and is valid only until the next call to `encode()` on that
// same encoder instance (or until the encoder is destroyed).
// Callers must consume the frame before invoking `encode()` again.
struct EncodedFrame {
    const void* data = nullptr;
    size_t      size = 0;
};

// IEncoder — outbound wire encoder interface.
//
// Each implementation accepts an internal TaskMessage and serialises it into
// a complete Raven wire frame (NetworkHeader + payload) ready for a single
// TCP write.
class IEncoder {
public:
    virtual ~IEncoder() = default;

    // Populate `out` with the full wire frame derived from `msg`.
    // Returns true on success, false if the message cannot be encoded.
    virtual bool encode(const TaskMessage& msg, EncodedFrame& out) const = 0;
};

// RawEncoder — assembles a complete wire frame from the TaskMessage.
//
// Prepends a NetworkHeader to the raw data buffer already stored in `msg`,
// producing a single contiguous frame ready for transmission.
//
// Thread-safety: each RawEncoder instance maintains an internal mutable buffer.
// Do not share a single instance across multiple tasks or call `encode()` from
// multiple threads concurrently.  In the typical usage pattern, each
// NetworkClientGateway owns its encoders and dispatches through a single task,
// so this constraint is satisfied automatically.
class RawEncoder final : public IEncoder {
public:
    bool encode(const TaskMessage& msg, EncodedFrame& out) const override
    {
        const size_t frame_size = sizeof(NetworkHeader) + msg.payload_size;
        buffer_.resize(frame_size);

        auto* hdr = reinterpret_cast<NetworkHeader*>(buffer_.data());
        hdr->msg_id       = msg.id;
        hdr->payload_size = static_cast<uint32_t>(msg.payload_size);

        if (msg.payload_size > 0 && msg.data != nullptr) {
            std::memcpy(buffer_.data() + sizeof(NetworkHeader), msg.data, msg.payload_size);
        }

        out.data = buffer_.data();
        out.size = frame_size;
        return true;
    }

private:
    mutable std::vector<uint8_t> buffer_;
};

} // namespace raven
