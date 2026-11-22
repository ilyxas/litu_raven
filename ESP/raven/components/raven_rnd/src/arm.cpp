#include "raven_rnd/arm.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace raven::rnd {
Snapshot OperationState::current() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}
Snapshot OperationState::find(uint32_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!id || current_.operation == id) return current_;
    for (const auto& entry : history_) if (entry.operation == id) return entry;
    return {id, Status::Unknown, 0, 0, false, 0};
}
void OperationState::publish(const Snapshot& value) {
    std::lock_guard<std::mutex> lock(mutex_);
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

void SimArmDriver::begin(float target, uint64_t generation, uint64_t now) {
    target_ = target;
    generation_ = generation;
    previous_ = now;
    moving_ = true;
}
void SimArmDriver::advance(uint64_t now) {
    if (!moving_) return;
    const float distance = static_cast<float>(now - previous_) * 0.09f;
    previous_ = now;
    if (mode_ == DriverMode::Silent || mode_ == DriverMode::Stalled || mode_ == DriverMode::Error) return;
    const float difference = target_ - position_;
    if (std::fabs(difference) <= distance) { position_ = target_; moving_ = false; }
    else position_ += difference > 0 ? distance : -distance;
    if (!moving_ && mode_ == DriverMode::Callback && callback_)
        callback_(context_, Completion{generation_, position_});
}
DriverStatus SimArmDriver::read() const {
    return {position_, mode_ != DriverMode::Silent,
            mode_ != DriverMode::Callback && std::fabs(position_ - target_) < 0.01f && !moving_,
            mode_ == DriverMode::Error};
}

ArmService::ArmService(IArmDriver& driver) : driver_(driver) {
    const bool configured = inbox.subscribe(Move, sizeof(Goal), Policy::Ordered) &&
        inbox.subscribe(Control, sizeof(Goal), Policy::Ordered) &&
        inbox.subscribe(Target, sizeof(TargetValue), Policy::Latest) &&
        inbox.subscribe(Cut, sizeof(Empty), Policy::Critical) &&
        inbox.subscribe(Configure, sizeof(Configuration), Policy::Ordered) &&
        inbox.subscribe(DriverDone, sizeof(Completion), Policy::Ordered) &&
        inbox.subscribe(CancelChildren, sizeof(ChildCancellation), Policy::Critical);
    assert(configured);
    (void)configured;
    driver_.set_callback(callback, this);
}
void ArmService::callback(void* context, const Completion& completion) {
    auto& self = *static_cast<ArmService*>(context);
    // Foreign callback context only copies data; owner tick handles timeout even
    // if callback delivery is lost to overload. No state mutation here.
    self.inbox.post(Message::make(DriverDone, 0, completion));
}
void ArmService::finish(Status status, uint64_t now) {
    if (active_.status != Status::Running) return;
    driver_.stop();
    moving_ = false;
    active_.status = status;
    active_.changed_ms = now;
    ++generation_;
    state.publish(active_);
}
void ArmService::reject(uint32_t id, float target, Status status, uint64_t now) {
    if (state.find(id).status != Status::Unknown) return;
    state.publish({id, status, active_.position, target, false, now});
}
void ArmService::retarget(float target, uint64_t now) {
    active_.target = target;
    active_.changed_ms = now;
    // Device-specific motion budget: simulated 90 degrees/s + 500 ms margin.
    deadline_ = now + static_cast<uint64_t>(std::fabs(target - active_.position) / 0.09f) + 500;
    moving_ = true;
    driver_.begin(target, ++generation_, now);
    state.publish(active_);
}
void ArmService::start(uint32_t id, float target, bool continuous, uint64_t now) {
    if (state.find(id).status != Status::Unknown) return; // Retained request deduplication.
    if (!id || !std::isfinite(target) || target < -180 || target > 180) {
        reject(id, target, Status::Rejected, now);
        return;
    }
    finish(Status::Superseded, now);
    active_ = {id, Status::Running, active_.position, target, continuous, now};
    retarget(target, now);
}
void ArmService::on_message(const Message& message, uint64_t now) {
    if (message.id == Cut) {
        cutoff_ = message.sequence;
        driver_.stop();
        finish(Status::Halted, now);
        return;
    }
    if (message.id == CancelChildren) {
        ChildCancellation cancellation;
        if (message.read(cancellation)) {
            child_cutoff_ = std::max(child_cutoff_, cancellation.through);
            if (child_cancelled(active_.operation)) finish(Status::Halted, now);
        }
        return;
    }
    if (message.id == Move || message.id == Control) {
        Goal goal;
        if (!message.read(goal)) return;
        if (message.sequence < cutoff_ || child_cancelled(message.request))
            reject(message.request, goal.target, Status::Halted, now);
        else {
            start(message.request, goal.target, message.id == Control, now);
            // A latest-value lane can be serviced before the queued Control
            // that establishes its operation. Preserve that causally newer input.
            if (message.id == Control && active_.status == Status::Running && active_.continuous &&
                active_.operation == latest_target_.operation && latest_target_sequence_ > message.sequence)
                retarget(latest_target_.target, now);
        }
    } else if (message.id == Target) {
        TargetValue value;
        if (message.sequence > cutoff_ && message.read(value) && std::isfinite(value.target) &&
            value.target >= -180 && value.target <= 180) {
            latest_target_ = value;
            latest_target_sequence_ = message.sequence;
            if (active_.status == Status::Running && active_.continuous && value.operation == active_.operation &&
                value.target != active_.target) retarget(value.target, now);
        }
    } else if (message.id == Configure) {
        Configuration config;
        if (message.read(config)) driver_.configure(config.mode);
    } else if (message.id == DriverDone) {
        Completion completion;
        if (message.read(completion) && completion.generation == generation_ &&
            active_.status == Status::Running) {
            active_.position = completion.position;
            moving_ = false;
            if (!active_.continuous) finish(Status::Succeeded, now);
            else state.publish(active_);
        }
    }
}
void ArmService::on_tick(uint64_t now) {
    if (active_.status != Status::Running) return;
    driver_.advance(now);
    const auto status = driver_.read();
    if (status.ready) active_.position = status.position;
    if (status.fault) { finish(Status::Failed, now); return; }
    if (status.ready && status.reached) {
        moving_ = false;
        if (!active_.continuous) { finish(Status::Succeeded, now); return; }
    }
    if (moving_ && now >= deadline_) { finish(Status::TimedOut, now); return; }
    state.publish(active_);
}

DualArmActivity::DualArmActivity(Dispatcher& dispatcher, const OperationState& left, const OperationState& right)
    : dispatcher_(dispatcher), left_(left), right_(right) {
    const bool configured = inbox.subscribe(Pair, sizeof(PairGoal), Policy::Ordered) &&
                            inbox.subscribe(Cut, sizeof(Empty), Policy::Critical);
    assert(configured);
    (void)configured;
}
void DualArmActivity::on_message(const Message& message, uint64_t now) {
    if (message.id == Cut) {
        cutoff_ = message.sequence;
        // The direct group cut reaches both arms, but a Pair already executing
        // can enqueue its children after that delivery. Cancel this Activity's
        // previously allocated range as well, without touching newer external
        // commands. Monotonically increasing cancellation ranges can coalesce.
        dispatcher_.deliver(3, Message::make(CancelChildren, 0, ChildCancellation{next_id_ - 1}));
        if (active_.status == Status::Running) {
            active_.status = Status::Halted;
            active_.changed_ms = now;
            state.publish(active_);
        }
        return;
    }
    PairGoal goal;
    if (message.id != Pair || !message.read(goal) || state.find(message.request).status != Status::Unknown) return;
    if (message.sequence < cutoff_) {
        state.publish({message.request, Status::Halted, 0, 0, false, now});
        return;
    }
    if (active_.status == Status::Running) {
        active_.status = Status::Superseded;
        active_.changed_ms = now;
        state.publish(active_);
    }
    left_id_ = next_id_++;
    right_id_ = next_id_++;
    const auto left = dispatcher_.deliver(1, Message::make(Move, left_id_, Goal{goal.left}));
    const auto right = dispatcher_.deliver(2, Message::make(Move, right_id_, Goal{goal.right}));
    active_ = {message.request, Status::Running, 0, 0, false, now};
    if (!left.complete() || !right.complete()) {
        // Fanout is explicitly non-transactional. Accepted children still run;
        // orchestration does not issue a broad cut against unrelated work.
        active_.status = Status::Rejected;
    }
    state.publish(active_);
}
void DualArmActivity::on_tick(uint64_t now) {
    if (active_.status != Status::Running) return;
    const auto left = left_.find(left_id_);
    const auto right = right_.find(right_id_);
    if ((terminal(left.status) && left.status != Status::Succeeded) ||
        (terminal(right.status) && right.status != Status::Succeeded)) {
        active_.status = Status::Failed;
        // Independent sibling execution continues; only an explicit critical
        // command cuts it. In particular, don't cancel another caller's goal.
    } else if (left.status == Status::Succeeded && right.status == Status::Succeeded) {
        active_.status = Status::Succeeded;
    } else return;
    active_.changed_ms = now;
    state.publish(active_);
}
} // namespace raven::rnd
