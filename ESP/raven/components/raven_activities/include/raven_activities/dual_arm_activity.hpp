#pragma once
#include "raven_core/component.hpp"
#include "raven_core/dispatcher.hpp"
#include "raven_contracts/arm_commands.hpp"
#include "raven_state/operation_state.hpp"

namespace raven::demo {
using namespace raven::runtime;
// Orchestration observes child statuses. It owns no actuator deadlines.
class DualArmActivity final : public Component {
public:
    DualArmActivity(Dispatcher& dispatcher, OperationState& left, OperationState& right);
    OperationState state;
    void on_message(const Message& message, uint64_t now) override;
    void on_tick(uint64_t now) override;
private:
    void release_children();
    Dispatcher& dispatcher_;
    OperationState& left_;
    OperationState& right_;
    OperationState::ResultLease left_result_, right_result_;
    Status left_outcome_{Status::Unknown}, right_outcome_{Status::Unknown};
    Snapshot active_{};
    uint32_t left_id_{}, right_id_{}, next_id_{first_child_id};
    uint64_t cutoff_{};
};
} // namespace raven::demo
