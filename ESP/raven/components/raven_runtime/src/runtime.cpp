#include "raven_runtime/runtime.hpp"

namespace raven::runtime {
bool Inbox::subscribe(uint16_t id, uint8_t size, Policy policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size > Message{}.payload.size() || subscription_count_ == subscriptions_.size()) return false;
    for (size_t i = 0; i < subscription_count_; ++i)
        if (subscriptions_[i].id == id) return false;
    subscriptions_[subscription_count_++] = {id, size, policy, false, {}};
    return true;
}

void Inbox::set_wake(Wake wake, void* context) {
    std::lock_guard<std::mutex> lock(mutex_);
    wake_ = wake;
    context_ = context;
}

Delivery Inbox::post(const Message& input) {
    Wake wake = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        Subscription* subscription = nullptr;
        for (size_t i = 0; i < subscription_count_; ++i)
            if (subscriptions_[i].id == input.id) subscription = &subscriptions_[i];
        if (!subscription) return Delivery::UnknownMessage;
        if (input.size != subscription->size) return Delivery::Invalid;
        if (subscription->policy == Policy::Ordered && count_ == capacity) return Delivery::Full;
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
    if (wake) wake(context); // Never call another component while holding the lock.
    return Delivery::Accepted;
}

bool Inbox::take_slot(Message& message, Policy policy) {
    Subscription* selected = nullptr;
    for (size_t i = 0; i < subscription_count_; ++i) {
        auto& slot = subscriptions_[i];
        if (slot.pending && slot.policy == policy &&
            (!selected || slot.message.sequence < selected->message.sequence)) selected = &slot;
    }
    if (!selected) return false;
    message = selected->message;
    selected->pending = false;
    return true;
}

bool Inbox::take_critical(Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    return take_slot(message, Policy::Critical);
}

bool Inbox::take_normal(Message& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((latest_turn_ || count_ == 0) && take_slot(message, Policy::Latest)) {
        latest_turn_ = false;
        return true;
    }
    if (count_ == 0) return false;
    message = queue_[head_];
    head_ = (head_ + 1) % capacity;
    --count_;
    latest_turn_ = true;
    return true;
}

size_t Inbox::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t result = count_;
    for (size_t i = 0; i < subscription_count_; ++i) result += subscriptions_[i].pending;
    return result;
}

bool Dispatcher::bind(uint8_t endpoint, Inbox& inbox) {
    if (endpoint >= endpoints_.size() || endpoints_[endpoint]) return false;
    endpoints_[endpoint] = &inbox;
    return true;
}

DeliveryReport Dispatcher::deliver(uint8_t destinations, const Message& message) const {
    DeliveryReport report{destinations, 0, 0, Delivery::Accepted};
    if (!destinations) report.reason = Delivery::Invalid;
    for (uint8_t i = 0; i < endpoints_.size(); ++i) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if (!(destinations & bit)) continue;
        const auto result = endpoints_[i] ? endpoints_[i]->post(message) : Delivery::UnknownMessage;
        if (result == Delivery::Accepted) report.accepted |= bit;
        else { report.rejected |= bit; report.reason = result; }
    }
    return report;
}

void Executor::step(uint64_t now_ms) {
    Message message;
    // At most the fixed subscription count: continuous critical ingress cannot
    // indefinitely starve timeout/status servicing.
    for (size_t i = 0; i < 8 && component_.inbox.take_critical(message); ++i)
        component_.on_message(message, now_ms);
    if (now_ms >= next_tick_) {
        component_.on_tick(now_ms);
        next_tick_ = now_ms - now_ms % period_ + period_;
    }
    if (component_.inbox.take_critical(message)) component_.on_message(message, now_ms);
    else if (component_.inbox.take_normal(message)) component_.on_message(message, now_ms);
}
} // namespace raven::runtime
