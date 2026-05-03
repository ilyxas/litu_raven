#include "raven_services/navigation_service.hpp"
#include "raven_core/task_message.hpp"
#include "raven_services/navigation_messages.hpp"
#include "raven_events/event_bus.hpp"
#include "raven_events/navigation_events.hpp"

#include "esp_log.h"

namespace raven {

static const char* TAG = "NavigationService";

NavigationService::NavigationService(NavigationState& state)
    : BaseService({ "nav_service", 2048, 5, 8 }), state_(state)
{
}

void NavigationService::handle_message(const TaskMessage& msg)
{
    if (msg.kind == nav_msg::KIND && msg.id == nav_msg::MOVE_FORWARD) {
        handle_move_forward();
    }
}

void NavigationService::handle_move_forward()
{
    // Stub: increment position by one unit forward along x.
    // No real physics, hardware IO, or path planning.
    //
    // NavigationService is the sole writer to NavigationState, so this
    // non-atomic read-modify-write cannot produce a lost update.
    // Readers always observe a fully-stored atomic value.
    state_.x.store(state_.x.load() + 1.0f);

    ESP_LOGI(TAG, "move_forward: position x=%.1f y=%.1f z=%.1f",
             state_.x.load(), state_.y.load(), state_.z.load());

    // Publish completion event so subscribers (e.g. NavigationActivity) can react.
    EventBus::post(NAVIGATION_EVENTS, NAV_EVENT_MOVE_FORWARD_DONE,
                   nullptr, 0, portMAX_DELAY);
}

} // namespace raven
