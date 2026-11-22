#pragma once
#include "raven_core/executor.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace raven {
// FreeRTOS host for one Component/Executor. Start once, before any producer.
// Static application lifetime: owner, task and inbox wake hook outlive producers.
// Handlers stay serialized; finite batches let other tasks run under input flood.
class OwnerTask {
public:
    OwnerTask(runtime::Component& component, const char* name, UBaseType_t priority, uint32_t period_ms = 10)
        : component_(component), executor_(component, period_ms), name_(name), priority_(priority), period_(period_ms ? period_ms : 1) {}
    OwnerTask(const OwnerTask&) = delete;
    OwnerTask& operator=(const OwnerTask&) = delete;
    void start();
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }
private:
    void run();
    runtime::Component& component_;
    runtime::Executor executor_;
    const char* name_;
    UBaseType_t priority_;
    uint32_t period_;
    TaskHandle_t handle_{};
};
} // namespace raven
