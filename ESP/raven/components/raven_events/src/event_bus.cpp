#include "raven_events/event_bus.hpp"

namespace raven {

esp_err_t EventBus::init()
{
    return esp_event_loop_create_default();
}

esp_err_t EventBus::post(esp_event_base_t base, int32_t id,
                         const void* data, size_t data_size,
                         TickType_t ticks_to_wait)
{
    return esp_event_post(base, id, data, data_size, ticks_to_wait);
}

esp_err_t EventBus::subscribe(esp_event_base_t base, int32_t id,
                               esp_event_handler_t handler, void* ctx)
{
    return esp_event_handler_register(base, id, handler, ctx);
}

esp_err_t EventBus::unsubscribe(esp_event_base_t base, int32_t id,
                                esp_event_handler_t handler)
{
    return esp_event_handler_unregister(base, id, handler);
}

} // namespace raven
