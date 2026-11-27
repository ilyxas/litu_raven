#pragma once
#include "raven_core/message.hpp"
#include <mutex>

namespace raven::runtime {
// Fixed storage. Critical and latest-value slots are reserved at subscription
// time. Only explicitly coalescible messages may use either of these policies.
class Inbox {
  public:
    static constexpr size_t capacity = 8;
    using Wake = void (*)(void *);
    bool subscribe(uint16_t id, uint8_t size, Policy policy);
    void set_wake(Wake wake, void *context); // Bootstrap only, before producers.
    Delivery post(const Message &message);
    bool take_critical(Message &message);
    bool take_normal(Message &message);
    size_t pending() const;

  private:
    struct Subscription {
        uint16_t id{};
        uint8_t size{};
        Policy policy{};
        bool pending{};
        Message message{};
    };
    bool take_slot(Message &message, Policy policy);
    mutable std::mutex mutex_;
    std::array<Subscription, 8> subscriptions_{};
    size_t subscription_count_{};
    std::array<Message, capacity> queue_{};
    size_t head_{}, count_{};
    uint64_t sequence_{};
    bool latest_turn_{};
    Wake wake_{};
    void *context_{};
};
} // namespace raven::runtime
