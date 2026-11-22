#include "raven_services/arm_service.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace raven::demo {
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
    sample_motion(now);
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
    // Account for the old trajectory before replacing its clock/generation.
    // A completion emitted here remains correlated with the old generation.
    sample_motion(now);
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
        if (active_.status == Status::Running) finish(Status::Halted, now);
        else driver_.stop();
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
DriverStatus ArmService::sample_motion(uint64_t now) {
    driver_.advance(now);
    const auto status = driver_.read();
    if (status.ready) active_.position = status.position;
    return status;
}
void ArmService::on_tick(uint64_t now) {
    if (active_.status != Status::Running) return;
    const auto status = sample_motion(now);
    if (status.fault) { finish(Status::Failed, now); return; }
    if (status.ready && status.reached) {
        moving_ = false;
        if (!active_.continuous) { finish(Status::Succeeded, now); return; }
    }
    if (moving_ && now >= deadline_) { finish(Status::TimedOut, now); return; }
    state.publish(active_);
}
} // namespace raven::demo
