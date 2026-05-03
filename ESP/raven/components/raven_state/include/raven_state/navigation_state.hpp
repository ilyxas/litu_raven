#pragma once

#include <atomic>
#include <mutex>

namespace raven {

// NavigationState — shared position truth updated by services.
//
// Services write into this state; Activities and other consumers read from it.
// Scalar position fields use atomics for lightweight lock-free access.
// The joystick snapshot is a multi-field struct guarded by a mutex so that
// readers always observe a consistent (x, y, timestamp) triple.
struct NavigationState {
    std::atomic<float> x{0.0f};
    std::atomic<float> y{0.0f};
    std::atomic<float> z{0.0f};

    // Latest joystick input, written by PilotInputService.
    struct JoystickData {
        float    x{0.0f};
        float    y{0.0f};
        uint32_t timestamp_ms{0};
    };

    // Write all three joystick fields atomically under the state lock.
    void set_joystick(float jx, float jy, uint32_t ts_ms)
    {
        std::unique_lock<std::mutex> lock(joystick_mutex_);
        joystick_.x            = jx;
        joystick_.y            = jy;
        joystick_.timestamp_ms = ts_ms;
    }

    // Read a consistent joystick snapshot under the state lock.
    JoystickData get_joystick() const
    {
        std::unique_lock<std::mutex> lock(joystick_mutex_);
        return joystick_;
    }

private:
    mutable std::mutex joystick_mutex_;
    JoystickData       joystick_;
};

} // namespace raven
