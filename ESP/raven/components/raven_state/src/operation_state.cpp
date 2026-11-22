#include "raven_state/operation_state.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace raven::demo {
Snapshot OperationState::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}
Snapshot OperationState::find(uint32_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return find_locked(id);
}
Snapshot OperationState::find_locked(uint32_t id) const {
    if (!id || current_.operation == id) return current_;
    for (const auto& entry : history_) if (entry.operation == id) return entry;
    return {id, Status::Unknown, 0, 0, false, 0};
}
OperationState::ResultLease OperationState::retain(uint32_t id) {
    if (!id) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    Snapshot value = find_locked(id);
    for (const auto& entry : retained_)
        if (entry.used && entry.value.operation == id) { value = entry.value; break; }
    for (size_t i = 0; i < retained_.size(); ++i) {
        if (!retained_[i].used) {
            retained_[i] = {true, value};
            return ResultLease(*this, i);
        }
    }
    return {};
}
OperationState::ResultLease::~ResultLease() { reset(); }
OperationState::ResultLease::ResultLease(ResultLease&& other) noexcept
    : owner_(std::exchange(other.owner_, nullptr)), slot_(other.slot_) {}
OperationState::ResultLease& OperationState::ResultLease::operator=(ResultLease&& other) noexcept {
    if (this != &other) {
        reset();
        owner_ = std::exchange(other.owner_, nullptr);
        slot_ = other.slot_;
    }
    return *this;
}
Snapshot OperationState::ResultLease::snapshot() const {
    if (!owner_) return {};
    std::lock_guard<std::mutex> lock(owner_->mutex_);
    return owner_->retained_[slot_].value;
}
void OperationState::ResultLease::reset() {
    if (!owner_) return;
    std::lock_guard<std::mutex> lock(owner_->mutex_);
    owner_->retained_[slot_].used = false;
    owner_ = nullptr;
}
void OperationState::publish(const Snapshot& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : retained_)
        if (entry.used && entry.value.operation == value.operation && !terminal(entry.value.status))
            entry.value = value;
    // Keep rejected/queued-after-cut operations from hiding a still running one.
    if (terminal(value.status)) {
        bool found = false;
        for (auto& entry : history_) {
            if (entry.operation == value.operation) { entry = value; found = true; break; }
        }
        if (!found) { history_[next_] = value; next_ = (next_ + 1) % history_capacity; }
    }
    if (current_.status != Status::Running || current_.operation == value.operation ||
        value.status == Status::Running) current_ = value;
}
} // namespace raven::demo
