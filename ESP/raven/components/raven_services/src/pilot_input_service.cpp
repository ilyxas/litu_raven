#include "raven_services/pilot_input_service.hpp"
#include "raven_services/pilot_messages.hpp"

#include <inttypes.h>
#include "esp_log.h"

namespace raven {

static const char* TAG = "PilotInputService";

PilotInputService::PilotInputService(NavigationState& state)
    : BaseService({ "pilot_input", 2048, 5, 8 }), state_(state)
{
}

void PilotInputService::handle_message(const TaskMessage& msg)
{
    if (msg.id != net_msg::JOYSTICK_INPUT) {
        return;
    }

    if (msg.data == nullptr || msg.payload_size < sizeof(JoystickPayload)) {
        ESP_LOGW(TAG, "JOYSTICK_INPUT: invalid payload (data=%p size=%zu)",
                 msg.data, msg.payload_size);
        return;
    }

    const auto* js = static_cast<const JoystickPayload*>(msg.data);

    // Update navigation state under the joystick mutex so readers always
    // observe a consistent (x, y, timestamp) triple.
    state_.set_joystick(js->x, js->y, js->timestamp_ms);

    ESP_LOGI(TAG, "joystick update: x=%.3f y=%.3f ts=%" PRIu32,
             js->x, js->y, js->timestamp_ms);
}

} // namespace raven
