#pragma once
#include "freertos/FreeRTOS.h"
BaseType_t xTaskCreate(void (*entry)(void *), const char *name, uint32_t stack, void *context,
                       UBaseType_t priority, TaskHandle_t *handle);
void vTaskDelay(TickType_t ticks);
inline unsigned uxTaskGetStackHighWaterMark(TaskHandle_t) {
    return 4096;
}
