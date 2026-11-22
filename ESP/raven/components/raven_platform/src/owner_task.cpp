#include "raven_platform/owner_task.hpp"
#include "raven_platform/clock.hpp"
#include "esp_check.h"

namespace raven {
void OwnerTask::start() {
    ESP_ERROR_CHECK(handle_ == nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    component_.inbox.set_wake([](void* ctx) {
        auto& self = *static_cast<OwnerTask*>(ctx);
        if (self.handle_) xTaskNotifyGive(self.handle_);
    }, this);
    const auto result = xTaskCreate([](void* ctx) { static_cast<OwnerTask*>(ctx)->run(); },
                                   name_, 4096, this, priority_, &handle_);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void OwnerTask::run() {
    unsigned batch = 0;
    for (;;) {
        executor_.step(now_ms());
        if (component_.inbox.pending() && ++batch < 8) continue;
        if (component_.inbox.pending()) vTaskDelay(1); // Give idle/other owners CPU under sustained ingress.
        else ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(period_) ? pdMS_TO_TICKS(period_) : 1);
        batch = 0;
    }
}
} // namespace raven
