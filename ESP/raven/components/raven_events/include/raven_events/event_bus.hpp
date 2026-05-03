#pragma once

#include "esp_event.h"
#include "esp_err.h"

namespace raven {

// EventBus — thin wrapper around the ESP-IDF default event loop.
//
// Events communicate broadcast facts about things that have happened.
// Directed commands belong in task queues (see BaseTask::post_message).
//
// Event handler callbacks must remain thin. Business logic should be
// posted to the owning task's queue and processed there, not executed
// directly inside a handler.
class EventBus {
public:
    // Initialise the ESP-IDF default event loop. Call once at startup.
    static esp_err_t init();

    // Post an event to the default event loop.
    // ticks_to_wait is the maximum time to block if the loop queue is full.
    static esp_err_t post(esp_event_base_t base, int32_t id,
                          const void* data, size_t data_size,
                          TickType_t ticks_to_wait = portMAX_DELAY);

    // Register a handler for a specific event base and id.
    // Pass ESP_EVENT_ANY_ID to receive all events for the given base.
    static esp_err_t subscribe(esp_event_base_t base, int32_t id,
                               esp_event_handler_t handler, void* ctx = nullptr);

    // Unregister a previously registered handler.
    static esp_err_t unsubscribe(esp_event_base_t base, int32_t id,
                                 esp_event_handler_t handler);
};

} // namespace raven
