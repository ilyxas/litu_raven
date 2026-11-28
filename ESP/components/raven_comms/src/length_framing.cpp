#include "raven_comms/length_framing.hpp"
#include <cstring>
namespace raven::comms {
bool LengthFrameReader::feed(const uint8_t *bytes, size_t size, uint64_t now, Sink sink, void *context) {
    if (failed_ || (!bytes && size) || !sink || expired(now)) {
        failed_ = true;
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        if (!used_)
            started_ = now;
        buffer_[used_++] = bytes[i];
        if (used_ == 2) {
            expected_ = (size_t(buffer_[0]) << 8) | buffer_[1];
            if (!expected_ || expected_ > icd::max_packet) {
                failed_ = true;
                return false;
            }
        }
        if (expected_ && used_ == expected_ + 2) {
            if (!sink(context, buffer_.data() + 2, expected_, now)) {
                failed_ = true;
                return false;
            }
            reset();
        }
    }
    return true;
}
size_t length_frame(const uint8_t *packet, size_t size, uint8_t *out, size_t capacity) {
    if (!out || !packet || !size || size > icd::max_packet || capacity < size + 2)
        return 0;
    std::memcpy(out + 2, packet, size);
    out[0] = uint8_t(size >> 8);
    out[1] = uint8_t(size);
    return size + 2;
}
} // namespace raven::comms
