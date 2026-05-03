#include "raven_activities/navigation_activity.hpp"
#include "raven_core/task_message.hpp"
#include "raven_services/navigation_messages.hpp"
#include "raven_services/pilot_messages.hpp"
#include "raven_events/event_bus.hpp"
#include "raven_events/navigation_events.hpp"

#include <inttypes.h>
#include "esp_log.h"

namespace raven {

static const char* TAG = "NavigationActivity";

// Poll joystick state every 100 ms while in Manual mode.
static constexpr TickType_t kManualTickInterval = pdMS_TO_TICKS(10000);

NavigationActivity::NavigationActivity(NavigationService& nav_service,
                                       NavigationState&   nav_state)
    : BaseActivity({ "nav_activity", 4096, 5, 8 })
    , nav_service_(nav_service)
    , nav_state_(nav_state)
{
}

void NavigationActivity::on_start()
{
    // Subscribe to NavigationService completion events.
    // The callback remains thin: it only translates the event into a self-message.
    EventBus::subscribe(NAVIGATION_EVENTS, NAV_EVENT_MOVE_FORWARD_DONE,
                        &NavigationActivity::on_nav_event, this);

    ESP_LOGI(TAG, "started — state: Idle");
}

void NavigationActivity::handle_message(const TaskMessage& msg)
{
    switch (msg.id) {
        case nav_msg::MOVE_FORWARD:         handle_move_forward();      break;
        case nav_msg::MOVE_DONE:            handle_move_done();         break;
        case net_msg::START_MANUAL_NAV:     handle_start_manual_nav();  break;
        case net_msg::HALT_MANUAL_NAV:      handle_halt_manual_nav();   break;
        default:                                                        break;
    }
}

void NavigationActivity::on_tick()
{
    // Called at kManualTickInterval while in Manual mode.
    // Read the latest joystick snapshot from NavigationState and act on it.
    const NavigationState::JoystickData js = nav_state_.get_joystick();

    ESP_LOGI(TAG, "Manual tick — joystick x=%.3f y=%.3f ts=%" PRIu32,
             js.x, js.y, js.timestamp_ms);

    // Demonstrate the intended architecture: activity polls state and could
    // command NavigationService here when a non-trivial input is present.
    // e.g. if (fabsf(js.x) > 0.1f || fabsf(js.y) > 0.1f) { ... }
}

void NavigationActivity::handle_start_manual_nav()
{
    if (state_ != State::Idle) {
        ESP_LOGW(TAG, "handle_start_manual_nav ignored — not Idle");
        return;
    }

    state_ = State::Manual;

    // Enable periodic polling of NavigationState for joystick input.
    set_tick_interval(kManualTickInterval);

    ESP_LOGI(TAG, "State — Idle → Manual (polling every %" PRIu32 " ms)",
             static_cast<uint32_t>(pdTICKS_TO_MS(kManualTickInterval)));
}

void NavigationActivity::handle_halt_manual_nav()
{
    if (state_ != State::Manual) {
        ESP_LOGW(TAG, "handle_halt_manual_nav ignored — not Manual");
        return;
    }

    state_ = State::Idle;

    // Disable periodic polling.
    set_tick_interval(0);

    ESP_LOGI(TAG, "State — Manual → Idle");
}

void NavigationActivity::handle_move_forward()
{
    if (state_ != State::Idle) {
        ESP_LOGW(TAG, "MoveForward ignored — not Idle");
        return;
    }

    ESP_LOGI(TAG, "MoveForward — Idle → Working");
    state_ = State::Working;

    // Delegate execution to NavigationService via a directed message.
    nav_service_.post_message({ nav_msg::KIND, nav_msg::MOVE_FORWARD, nullptr, 0 });
}

void NavigationActivity::handle_move_done()
{
    // Business logic runs here, in the activity's own task context — not
    // inside the event callback.
    ESP_LOGI(TAG, "MoveDone — Working → Idle");
    state_ = State::Idle;
}

// static
void NavigationActivity::on_nav_event(void* ctx, esp_event_base_t /*base*/,
                                       int32_t /*id*/, void* /*event_data*/)
{
    // Thin callback: translate the completion event into a directed self-message
    // and return immediately. All business logic executes in handle_move_done().
    auto* self = static_cast<NavigationActivity*>(ctx);
    self->post_message({ nav_msg::KIND, nav_msg::MOVE_DONE, nullptr, 0 });
}

} // namespace raven
