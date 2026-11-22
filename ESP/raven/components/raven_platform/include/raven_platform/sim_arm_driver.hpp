#pragma once
#include "raven_contracts/arm_driver.hpp"

namespace raven::demo {
// Deterministic virtual actuator; advance() samples motion on the owner clock.
// Failure modes exercise the service contract without touching GPIO.
class SimArmDriver final : public IArmDriver {
public:
    void begin(float target, uint64_t generation, uint64_t now) override;
    void stop() override { moving_ = false; }
    void advance(uint64_t now) override;
    DriverStatus read() const override;
    void configure(DriverMode mode) override { mode_ = mode; }
    void set_callback(Callback callback, void* context) override { callback_ = callback; context_ = context; }
    uint64_t generation() const { return generation_; }
private:
    DriverMode mode_{DriverMode::Polling};
    float position_{}, target_{};
    uint64_t previous_{}, generation_{};
    bool moving_{};
    Callback callback_{};
    void* context_{};
};
} // namespace raven::demo
