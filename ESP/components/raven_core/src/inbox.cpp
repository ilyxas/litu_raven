#include "raven_core/inbox.hpp"

namespace raven::runtime {
bool Inbox::subscribe(uint16_t id, uint8_t size, Policy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size > Message{}.payload.size() || subscription_count_ == subscriptions_.size())
        return false;
    for (size_t i = 0; i < subscription_count_; ++i)
        if (subscriptions_[i].id == id)
            return false;
    subscriptions_[subscription_count_++] = {id, size, policy, false, {}};
    return true;
}

void Inbox::set_wake(Wake wake, void *context) {
    std::lock_guard<std::mutex> lock(mutex_);
    wake_ = wake;
    context_ = context;
}

Delivery Inbox::post(const Message &input) {
    Wake wake = nullptr;
    void *context = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Subscription *subscription = nullptr;
        for (size_t i = 0; i < subscription_count_; ++i)
            if (subscriptions_[i].id == input.id)
                subscription = &subscriptions_[i];
        if (!subscription)
            return Delivery::UnknownMessage;
        if (input.size != subscription->size)
            return Delivery::Invalid;
        if (subscription->policy == Policy::Ordered && count_ == capacity)
            return Delivery::Full;
        Message message = input;
        message.sequence = ++sequence_;
        if (subscription->policy == Policy::Ordered) {
            queue_[(head_ + count_) % capacity] = message;
            ++count_;
        } else {
            subscription->message = message;
            subscription->pending = true;
        }
        wake = wake_;
        context = context_;
    }
    if (wake)
        wake(context); // Never call another component while holding the lock.
    return Delivery::Accepted;
}

bool Inbox::take_slot(Message &message, Policy policy) {
    Subscription *selected = nullptr;
    for (size_t i = 0; i < subscription_count_; ++i) {
        auto &slot = subscriptions_[i];
        if (slot.pending && slot.policy == policy &&
            (!selected || slot.message.sequence < selected->message.sequence))
            selected = &slot;
    }
    if (!selected)
        return false;
    message = selected->message;
    selected->pending = false;
    return true;
}

bool Inbox::take_critical(Message &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return take_slot(message, Policy::Critical);
}

bool Inbox::take_normal(Message &message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((latest_turn_ || count_ == 0) && take_slot(message, Policy::Latest)) {
        latest_turn_ = false;
        return true;
    }
    if (count_ == 0)
        return false;
    message = queue_[head_];
    head_ = (head_ + 1) % capacity;
    --count_;
    latest_turn_ = true;
    return true;
}

size_t Inbox::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t result = count_;
    for (size_t i = 0; i < subscription_count_; ++i)
        result += subscriptions_[i].pending;
    return result;
}
} // namespace raven::runtime
