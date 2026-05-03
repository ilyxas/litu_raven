#pragma once

#include "decoder.hpp"

#include <cstdint>
#include <vector>

namespace raven {

class DecoderRegistry {
public:
    bool register_decoder(uint16_t msg_id, const IDecoder* decoder) {
        if (decoder == nullptr) {
            return false;
        }

        for (const Entry& e : entries_) {
            if (e.msg_id == msg_id) {
                return false; // duplicate registration
            }
        }

        entries_.push_back({ msg_id, decoder });
        return true;
    }

    const IDecoder* find(uint16_t msg_id) const {
        for (const Entry& e : entries_) {
            if (e.msg_id == msg_id) {
                return e.decoder;
            }
        }
        return nullptr;
    }

private:
    struct Entry {
        uint16_t        msg_id;
        const IDecoder* decoder;
    };

    std::vector<Entry> entries_;
};

} // namespace raven