#pragma once
#include "raven_state/operation_state.hpp"
#include "raven_state/sensor_state.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

namespace raven::demo {
// Outbound TCP worker. Samples states without owning them; a slow receiver only
// delays this worker. Host string and referenced states have application lifetime.
class TelemetryPublisher {
public:
    TelemetryPublisher(const OperationState& left, const OperationState& right, const SensorState& sensors,
                       const char* host, uint16_t port)
        : left_(left), right_(right), sensors_(sensors), host_(host), port_(port) {}
    void start();
    void use_test_sink(bool enabled) { test_sink_ = enabled; }
    unsigned frames() const { return frames_; }
    unsigned failures() const { return failures_; }
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }
private:
    void run();
    const OperationState& left_;
    const OperationState& right_;
    const SensorState& sensors_;
    const char* host_;
    uint16_t port_;
    std::atomic<bool> test_sink_{false};
    std::atomic<unsigned> frames_{0}, failures_{0};
    TaskHandle_t handle_{};
};
} // namespace raven::demo
