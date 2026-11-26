#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace raven::runtime {
// Delivery and execution are intentionally separate contracts.
enum class Delivery : uint8_t { Accepted, UnknownMessage, Full, Invalid };
enum class Policy : uint8_t { Ordered, Latest, Critical };

// Owned payload copied at delivery; no borrowed pointers cross an inbox.
struct Message {
    static constexpr size_t payload_capacity = 64;
    uint16_t id{};
    uint32_t request{};
    uint64_t sequence{}; // Assigned by the destination inbox, never by the sender.
    uint8_t size{};
    std::array<uint8_t, payload_capacity> payload{};

    template <class T> static Message make(uint16_t id, uint32_t request, const T &value) {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= payload_capacity);
        Message message{};
        message.id = id;
        message.request = request;
        message.size = sizeof(T);
        std::memcpy(message.payload.data(), &value, sizeof(T));
        return message;
    }
    template <class T> bool read(T &value) const {
        static_assert(std::is_trivially_copyable_v<T>);
        if (size != sizeof(T))
            return false;
        std::memcpy(&value, payload.data(), sizeof(T));
        return true;
    }
};
} // namespace raven::runtime
