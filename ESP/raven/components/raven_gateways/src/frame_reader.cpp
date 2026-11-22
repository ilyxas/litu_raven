#include "raven_gateways/frame_reader.hpp"

namespace raven::demo {
bool FrameReader::feed(const uint8_t* data, size_t size, Emit emit, void* context) {
    for (size_t i = 0; i < size; ++i) {
        bytes_[used_++] = data[i];
        if (used_ == 6) {
            const uint32_t payload = wire::u32(bytes_.data() + 2);
            if (payload > wire::max_payload) return false;
            expected_ = 6 + payload;
        }
        if (used_ == expected_) {
            const uint16_t id = uint16_t(bytes_[0]) | uint16_t(bytes_[1]) << 8;
            const auto response = protocol_.handle(id, bytes_.data() + 6, expected_ - 6);
            if (!emit(context, response)) return false;
            used_ = 0;
            expected_ = 6;
        }
    }
    return true;
}
} // namespace raven::demo
