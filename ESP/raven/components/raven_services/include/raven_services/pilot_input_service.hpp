#pragma once

#include "raven_services/base_service.hpp"
#include "raven_state/navigation_state.hpp"

namespace raven {

// PilotInputService — dedicated receiver for joystick / pilot input messages.
//
// Receives JOYSTICK_INPUT messages routed from the network gateway, validates
// the payload, and writes the latest joystick snapshot into NavigationState
// under the state lock.
//
// NavigationActivity polls NavigationState during manual mode via the BaseTask
// periodic tick mechanism — this service is the sole writer for joystick data.
class PilotInputService : public BaseService {
public:
    explicit PilotInputService(NavigationState& state);

protected:
    void handle_message(const TaskMessage& msg) override;

private:
    NavigationState& state_;
};

} // namespace raven
