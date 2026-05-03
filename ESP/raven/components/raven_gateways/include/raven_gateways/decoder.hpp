#pragma once

#include "network_header.hpp"

#include <cstddef>
#include <cstdint>

namespace raven {

struct DecodedMessageView {
    uint16_t    kind = 0;
    uint16_t    id = 0;
    const void* payload = nullptr;
    size_t      payload_size = 0;
};

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual bool decode(
        const NetworkHeader& header,
        const uint8_t* payload_bytes,
        DecodedMessageView& out
    ) const = 0;
};

// Fixed-size binary payload validator.
// Good for structs like JoystickPayload, StartManualPayload, etc.
class FixedSizeDecoder final : public IDecoder {
public:
    FixedSizeDecoder(uint16_t kind, size_t expected_size)
        : kind_(kind), expected_size_(expected_size) {}

    bool decode(
        const NetworkHeader& header,
        const uint8_t* payload_bytes,
        DecodedMessageView& out
    ) const override
    {
        if (header.payload_size != expected_size_) {
            return false;
        }

        out.kind = kind_;
        out.id = header.msg_id;
        out.payload = payload_bytes;
        out.payload_size = header.payload_size;
        return true;
    }

private:
    uint16_t kind_;
    size_t   expected_size_;
};

// Variable-size payload validator.
// Good for strings, JSON blobs, LLM responses, chunk payloads, etc.
class VariableSizeDecoder final : public IDecoder {
public:
    VariableSizeDecoder(uint16_t kind, size_t min_size, size_t max_size)
        : kind_(kind), min_size_(min_size), max_size_(max_size) {}

    bool decode(
        const NetworkHeader& header,
        const uint8_t* payload_bytes,
        DecodedMessageView& out
    ) const override
    {
        if (header.payload_size < min_size_ || header.payload_size > max_size_) {
            return false;
        }

        out.kind = kind_;
        out.id = header.msg_id;
        out.payload = payload_bytes;
        out.payload_size = header.payload_size;
        return true;
    }

private:
    uint16_t kind_;
    size_t   min_size_;
    size_t   max_size_;
};

} // namespace raven