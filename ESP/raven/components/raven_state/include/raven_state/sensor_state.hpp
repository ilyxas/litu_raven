#pragma once
#include <cstdint>
#include <mutex>

namespace raven::demo {
struct SensorSnapshot { uint64_t sample{}; float temperature{}; };
// Single sensor owner publishes; telemetry takes a coherent copy without
// coupling its network latency to acquisition.
class SensorState {
public:
    SensorSnapshot current() const { std::lock_guard<std::mutex> lock(mutex_); return value_; }
    void publish(SensorSnapshot value) { std::lock_guard<std::mutex> lock(mutex_); value_ = value; }
private:
    mutable std::mutex mutex_;
    SensorSnapshot value_{};
};
} // namespace raven::demo
