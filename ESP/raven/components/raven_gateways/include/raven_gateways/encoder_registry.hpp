#pragma once

#include "encoder.hpp"

#include <cstdint>
#include <vector>

namespace raven {

// EncoderRegistry — maps (kind, id) pairs to outbound encoder instances.
//
// Mirrors DecoderRegistry on the inbound side.
// Encoders are registered before the gateway task starts and are accessed
// read-only inside the owning task context during message dispatch.
class EncoderRegistry {
public:
    // Register an encoder for the given (kind, id) pair.
    // Returns false if encoder is nullptr or a duplicate key is already registered.
    bool register_encoder(uint16_t kind, uint16_t id, const IEncoder* encoder)
    {
        if (encoder == nullptr) {
            return false;
        }

        for (const Entry& e : entries_) {
            if (e.kind == kind && e.id == id) {
                return false; // duplicate registration
            }
        }

        entries_.push_back({ kind, id, encoder });
        return true;
    }

    // Find the encoder registered for the given (kind, id) pair.
    // Returns nullptr if no encoder is registered for that key.
    const IEncoder* find(uint16_t kind, uint16_t id) const
    {
        for (const Entry& e : entries_) {
            if (e.kind == kind && e.id == id) {
                return e.encoder;
            }
        }
        return nullptr;
    }

private:
    struct Entry {
        uint16_t        kind;
        uint16_t        id;
        const IEncoder* encoder;
    };

    std::vector<Entry> entries_;
};

} // namespace raven
