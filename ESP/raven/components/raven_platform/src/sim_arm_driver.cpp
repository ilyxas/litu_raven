#include "raven_platform/sim_arm_driver.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace raven::demo {
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
} // namespace raven::demo
