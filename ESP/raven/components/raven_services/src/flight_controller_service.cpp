#include "raven_services/flight_controller_service.hpp"

#include "raven_events/event_bus.hpp"
#include "raven_events/flight_controller_events.hpp"

#include "esp_log.h"
#include "freertos/task.h"

namespace raven {
namespace {
constexpr const char* TAG = "FlightControllerService";
}

FlightControllerService::FlightControllerService(IBetaflightClient& client,
                                                 FlightControllerState& state,
                                                 const Config& config)
    : BaseService({config.task_name,
                   config.stack_size,
                   config.priority,
                   config.queue_length})
    , client_(client)
    , state_(state)
    , config_(config)
{
}

void FlightControllerService::on_start()
{
    set_tick_interval(config_.poll_interval);
    poll_link();
}

void FlightControllerService::handle_message(const TaskMessage& /*message*/)
{
    // The first product milestone is deliberately read-only. Product commands
    // will be added only with an explicit safety and ownership policy.
}

void FlightControllerService::on_tick()
{
    poll_link();
}

uint32_t FlightControllerService::now_ms()
{
    return static_cast<uint32_t>(pdTICKS_TO_MS(xTaskGetTickCount()));
}

void FlightControllerService::poll_link()
{
    const FlightControllerSnapshot before = state_.snapshot();

    if (!client_.is_open() && !client_.open()) {
        handle_failure();
        return;
    }

    if (before.link_status == FlightControllerLinkStatus::Disconnected) {
        BetaflightIdentity wire_identity;
        if (!client_.probe(wire_identity, config_.response_timeout_ms)) {
            handle_failure();
            return;
        }

        FlightControllerIdentity identity;
        identity.protocol_version = wire_identity.protocol_version;
        identity.api_major = wire_identity.api_major;
        identity.api_minor = wire_identity.api_minor;
        identity.variant = wire_identity.variant;
        identity.firmware_version = wire_identity.firmware_version;

        state_.mark_connected(identity, now_ms());
        publish_link_event(FC_EVENT_LINK_UP);

        ESP_LOGI(TAG,
                 "MSP link up: %.4s API %u.%u FC %u.%u.%u",
                 identity.variant.data(),
                 identity.api_major,
                 identity.api_minor,
                 identity.firmware_version[0],
                 identity.firmware_version[1],
                 identity.firmware_version[2]);
        return;
    }

    if (client_.ping(config_.response_timeout_ms)) {
        state_.record_success(now_ms());
        return;
    }

    handle_failure();
}

void FlightControllerService::handle_failure()
{
    const FlightControllerSnapshot before = state_.snapshot();
    state_.record_failure(now_ms());
    const FlightControllerSnapshot after = state_.snapshot();

    if (after.consecutive_failures < config_.disconnect_after_failures) {
        return;
    }

    client_.close();
    state_.mark_disconnected();

    if (before.link_status == FlightControllerLinkStatus::Connected) {
        publish_link_event(FC_EVENT_LINK_DOWN);
        ESP_LOGW(TAG, "MSP link down after %u consecutive failures",
                 static_cast<unsigned>(after.consecutive_failures));
    }
}

void FlightControllerService::publish_link_event(int32_t event_id)
{
    const esp_err_t result = EventBus::post(FLIGHT_CONTROLLER_EVENTS,
                                            event_id,
                                            nullptr,
                                            0,
                                            0);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "failed to publish FC link event: %s", esp_err_to_name(result));
    }
}

} // namespace raven
