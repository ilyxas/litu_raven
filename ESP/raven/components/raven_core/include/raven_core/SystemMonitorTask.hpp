#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace raven {

class SystemMonitorTask {
public:
    struct Config {
        const char* name = "system_monitor";
        uint32_t stack_size = 4096;
        UBaseType_t priority = 1;
        TickType_t period_ticks = pdMS_TO_TICKS(2000);
    };

    struct QueueWatch {
        const char* name;
        QueueHandle_t queue;
    };

    explicit SystemMonitorTask(const Config& config);
    ~SystemMonitorTask();

    void start();
    void stop();

    void watch_queue(const char* name, QueueHandle_t queue);

private:
    static void task_entry(void* arg);
    void run();

private:
    Config config_;
    TaskHandle_t handle_ = nullptr;
    bool stop_requested_ = false;
    std::vector<QueueWatch> watched_queues_;
};
} // namespace raven