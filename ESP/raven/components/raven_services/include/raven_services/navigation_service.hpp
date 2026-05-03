#pragma once

#include "raven_services/base_service.hpp"
#include "raven_state/navigation_state.hpp"

namespace raven {

// NavigationService — stub executor for navigation commands.
//
// Receives directed messages (posted to its inbound queue), performs a
// stub update to NavigationState, then publishes a completion event via
// EventBus so that subscribers (e.g. NavigationActivity) can react.
//
// No real physics, hardware IO, or path planning is performed here.
// This service exists to demonstrate the Activity ➜ Service ➜ State +
// EventBus communication pattern.
class NavigationService : public BaseService {
public:
    explicit NavigationService(NavigationState& state);

protected:
    void handle_message(const TaskMessage& msg) override;

private:
    void handle_move_forward();

    NavigationState& state_;
};

} // namespace raven
