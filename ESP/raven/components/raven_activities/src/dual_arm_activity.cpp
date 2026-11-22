#include "raven_activities/dual_arm_activity.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace raven::demo {
DualArmActivity::DualArmActivity(Dispatcher& dispatcher, OperationState& left, OperationState& right)
    : dispatcher_(dispatcher), left_(left), right_(right) {
    const bool configured = inbox.subscribe(Pair, sizeof(PairGoal), Policy::Ordered) &&
                            inbox.subscribe(Cut, sizeof(Empty), Policy::Critical);
    assert(configured);
    (void)configured;
}
void DualArmActivity::release_children() {
    left_result_.reset();
    right_result_.reset();
}
void DualArmActivity::on_message(const Message& message, uint64_t now) {
    if (message.id == Cut) {
        cutoff_ = message.sequence;
        // The direct group cut reaches both arms, but a Pair already executing
        // can enqueue its children after that delivery. Cancel this Activity's
        // previously allocated range as well, without touching newer external
        // commands. Monotonically increasing cancellation ranges can coalesce.
        dispatcher_.deliver(3, Message::make(CancelChildren, 0, ChildCancellation{next_id_ - 1}));
        release_children();
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
    release_children();
    left_outcome_ = right_outcome_ = Status::Unknown;
    left_result_ = left_.retain(left_id_);
    right_result_ = right_.retain(right_id_);
    active_ = {message.request, Status::Running, 0, 0, false, now};
    if (!left_result_ || !right_result_) {
        active_.status = Status::Rejected;
        release_children();
        state.publish(active_);
        return;
    }
    const auto left = dispatcher_.deliver(1, Message::make(Move, left_id_, Goal{goal.left}));
    const auto right = dispatcher_.deliver(2, Message::make(Move, right_id_, Goal{goal.right}));
    if (!left.complete() || !right.complete()) {
        // Fanout is explicitly non-transactional. Accepted children still run;
        // orchestration does not issue a broad cut against unrelated work.
        active_.status = Status::Rejected;
        release_children();
    }
    state.publish(active_);
}
void DualArmActivity::on_tick(uint64_t now) {
    if (active_.status != Status::Running) return;
    if (!terminal(left_outcome_)) {
        left_outcome_ = left_result_.snapshot().status;
        if (terminal(left_outcome_)) left_result_.reset();
    }
    if (!terminal(right_outcome_)) {
        right_outcome_ = right_result_.snapshot().status;
        if (terminal(right_outcome_)) right_result_.reset();
    }
    if ((terminal(left_outcome_) && left_outcome_ != Status::Succeeded) ||
        (terminal(right_outcome_) && right_outcome_ != Status::Succeeded)) {
        active_.status = Status::Failed;
        // Independent sibling execution continues; only an explicit critical
        // command cuts it. In particular, don't cancel another caller's goal.
    } else if (left_outcome_ == Status::Succeeded && right_outcome_ == Status::Succeeded) {
        active_.status = Status::Succeeded;
    } else return;
    active_.changed_ms = now;
    release_children();
    state.publish(active_);
}
} // namespace raven::demo
