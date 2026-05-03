#pragma once

#include "raven_activities/base_activity.hpp"
#include "raven_services/navigation_service.hpp"
#include "raven_state/navigation_state.hpp"

#include "esp_event.h"

namespace raven {

// NavigationActivity — reference activity demonstrating the Raven communication model.
//
// Owns an Idle/Working/Manual internal state machine.
//
// In Manual mode the activity uses the BaseTask periodic tick capability to poll
// NavigationState for the latest joystick snapshot.  Joystick ingress travels:
//   gateway → PilotInputService → NavigationState
// while this activity consumes it independently via on_tick(), demonstrating the
// noisy-ingress / polling-consumer architecture.
class NavigationActivity : public BaseActivity {
public:
    explicit NavigationActivity(NavigationService& nav_service,
                                NavigationState&   nav_state);

protected:
    // Subscribes to NavigationService completion events.
    void on_start() override;

    // Dispatches incoming directed messages and self-messages.
    void handle_message(const TaskMessage& msg) override;

    // Called at the configured tick interval while in Manual mode.
    // Reads the latest joystick snapshot from NavigationState and logs it.
    void on_tick() override;

private:
    enum class State { Idle, Working, Manual };

    void handle_move_forward();
    void handle_move_done();
    void handle_start_manual_nav();
    void handle_halt_manual_nav();

    // Thin EventBus callback — translates the event into a self-message only.
    static void on_nav_event(void* ctx, esp_event_base_t base,
                             int32_t id, void* event_data);

    NavigationService& nav_service_;
    NavigationState&   nav_state_;
    State              state_{State::Idle};
};

} // namespace raven
