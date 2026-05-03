#include "raven_core/SystemMonitorTask.hpp"

#include "esp_log.h"
#include "esp_heap_caps.h"

namespace raven {

SystemMonitorTask::SystemMonitorTask(const Config& config)
    : config_(config) {}

SystemMonitorTask::~SystemMonitorTask() {
    stop();
}

void SystemMonitorTask::start() {
    if (handle_ != nullptr) {
        return;
    }

    stop_requested_ = false;

    BaseType_t rc = xTaskCreate(
        &SystemMonitorTask::task_entry,
        config_.name,
        config_.stack_size,
        this,
        config_.priority,
        &handle_
    );

    configASSERT(rc == pdPASS);
}

void SystemMonitorTask::stop() {
    stop_requested_ = true;

    if (handle_ != nullptr) {
        vTaskDelete(handle_);
        handle_ = nullptr;
    }
}

void SystemMonitorTask::watch_queue(const char* name, QueueHandle_t queue) {
    if (name == nullptr || queue == nullptr) {
        return;
    }

    watched_queues_.push_back({ name, queue });
}

void SystemMonitorTask::task_entry(void* arg) {
    auto* self = static_cast<SystemMonitorTask*>(arg);
    self->run();
}

void SystemMonitorTask::run() {
    static const char* TAG = "SystemMonitor";

    while (!stop_requested_) {
        const size_t free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        const size_t largest_8bit = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        const size_t min_8bit = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

        ESP_LOGI(TAG,
                 "heap: free=%u largest=%u min=%u",
                 static_cast<unsigned>(free_8bit),
                 static_cast<unsigned>(largest_8bit),
                 static_cast<unsigned>(min_8bit));

        for (const auto& w : watched_queues_) {
            const UBaseType_t waiting = uxQueueMessagesWaiting(w.queue);
            ESP_LOGI(TAG,
                     "queue[%s]: waiting=%u",
                     w.name,
                     static_cast<unsigned>(waiting));
        }

        vTaskDelay(config_.period_ticks);
    }

    handle_ = nullptr;
    vTaskDelete(nullptr);
}

}