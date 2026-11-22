#pragma once
#include "raven_core/component.hpp"
#include "raven_contracts/arm_driver.hpp"
#include "raven_state/operation_state.hpp"

namespace raven::demo {
using namespace raven::runtime;
// Owns one actuator, its motion budget and final outcome. Producers post commands;
// readers poll state. Run it with one Executor/OwnerTask, never concurrent handlers.
class ArmService final : public Component {
public:
    explicit ArmService(IArmDriver& driver);
    OperationState state;
    void on_message(const Message& message, uint64_t now) override;
    void on_tick(uint64_t now) override;
private:
    static void callback(void* context, const Completion& completion);
    void start(uint32_t id, float target, bool continuous, uint64_t now);
    void retarget(float target, uint64_t now);
    void finish(Status status, uint64_t now);
    void reject(uint32_t id, float target, Status status, uint64_t now);
    DriverStatus sample_motion(uint64_t now);
    bool child_cancelled(uint32_t id) const { return id >= first_child_id && id <= child_cutoff_; }
    IArmDriver& driver_;
    Snapshot active_{};
    uint64_t generation_{}, deadline_{}, cutoff_{};
    TargetValue latest_target_{};
    uint64_t latest_target_sequence_{};
    uint32_t child_cutoff_{};
    bool moving_{};
};
} // namespace raven::demo
