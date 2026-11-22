#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <type_traits>

namespace raven::runtime {

// Delivery and execution are intentionally separate contracts.
enum class Delivery : uint8_t { Accepted, UnknownMessage, Full, Invalid };
enum class Policy : uint8_t { Ordered, Latest, Critical };

struct Message {
    uint16_t id{};
    uint32_t request{};
    uint64_t sequence{}; // Assigned by the destination inbox, never by the sender.
    uint8_t size{};
    std::array<uint8_t, 24> payload{};

    template<class T> static Message make(uint16_t id, uint32_t request, const T& value) {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= 24);
        Message message{};
        message.id = id;
        message.request = request;
        message.size = sizeof(T);
        std::memcpy(message.payload.data(), &value, sizeof(T));
        return message;
    }
    template<class T> bool read(T& value) const {
        static_assert(std::is_trivially_copyable_v<T>);
        if (size != sizeof(T)) return false;
        std::memcpy(&value, payload.data(), sizeof(T));
        return true;
    }
};

// Fixed storage. Critical and latest-value slots are reserved at subscription
// time. Only explicitly coalescible messages may use either of these policies.
class Inbox {
public:
    static constexpr size_t capacity = 8;
    using Wake = void (*)(void*);
    bool subscribe(uint16_t id, uint8_t size, Policy policy);
    void set_wake(Wake wake, void* context); // Bootstrap only, before producers.
    Delivery post(const Message& message);
    bool take_critical(Message& message);
    bool take_normal(Message& message);
    size_t pending() const;
private:
    struct Subscription {
        uint16_t id{};
        uint8_t size{};
        Policy policy{};
        bool pending{};
        Message message{};
    };
    bool take_slot(Message& message, Policy policy);
    mutable std::mutex mutex_;
    std::array<Subscription, 8> subscriptions_{};
    size_t subscription_count_{};
    std::array<Message, capacity> queue_{};
    size_t head_{}, count_{};
    uint64_t sequence_{};
    bool latest_turn_{};
    Wake wake_{};
    void* context_{};
};

struct DeliveryReport {
    uint8_t requested{}, accepted{}, rejected{};
    Delivery reason{Delivery::Accepted};
    bool complete() const { return requested != 0 && accepted == requested; }
};

class Dispatcher {
public:
    bool bind(uint8_t endpoint, Inbox& inbox); // Immutable once tasks start.
    DeliveryReport deliver(uint8_t destinations, const Message& message) const;
private:
    std::array<Inbox*, 8> endpoints_{};
};

class Component {
public:
    virtual ~Component() = default;
    Inbox inbox;
    virtual void on_message(const Message&, uint64_t now_ms) = 0;
    virtual void on_tick(uint64_t now_ms) = 0;
};

// Used by both FreeRTOS tasks and deterministic tests. Deadlines are phase
// based; missed ticks are coalesced rather than replayed in an unbounded loop.
class Executor {
public:
    Executor(Component& component, uint32_t period_ms)
        : component_(component), period_(period_ms ? period_ms : 1) {}
    void step(uint64_t now_ms);
private:
    Component& component_;
    uint32_t period_;
    uint64_t next_tick_{};
};
} // namespace raven::runtime
