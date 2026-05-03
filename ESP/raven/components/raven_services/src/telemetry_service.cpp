
#include "raven_services/telemetry_service.hpp"
#include "raven_services/telemetry_messages.hpp"
#include "raven_core/task_message.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <inttypes.h>

namespace raven {

static const char* TAG = "TelemetryService";

TelemetryService::TelemetryService(TelemetryState& state, NetworkClientGateway& gateway)
    : BaseService({ "telemetry_svc", 4096, 5, 8 })
    , state_(state)
    , gateway_(gateway)
{
}

void TelemetryService::on_start()
{
    set_tick_interval(kTickInterval);
    ESP_LOGI(TAG, "started — collecting every 100 ms");
}

void TelemetryService::handle_message(const TaskMessage& msg)
{
    if (msg.id != net_msg::TELEMETRY_ACTIVE) {
        return;
    }

    if (msg.data == nullptr || msg.payload_size < sizeof(TelemetryActivePayload)) {
        ESP_LOGW(TAG, "TELEMETRY_ACTIVE: invalid payload");
        return;
    }

    const auto* p = static_cast<const TelemetryActivePayload*>(msg.data);
    active_ = (p->active != 0);

    ESP_LOGI(TAG, "TELEMETRY_ACTIVE → %s", active_ ? "enabled" : "disabled");
}

void TelemetryService::on_tick()
{
    if (!active_) {
        return;
    }

    collect_navigation();
    collect_system();
    collect_sensor();
    transmit();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void TelemetryService::collect_navigation()
{
    TelemetryState::NavigationTelemetry nav;
    nav.speed      = 0.0f;
    nav.direction  = 0.0f;
    nav.position_x = 0.0f;
    nav.position_y = 0.0f;
    nav.timestamp  = static_cast<uint32_t>(xTaskGetTickCount());

    state_.set_navigation(nav);

    snapshot_.navigation.speed      = nav.speed;
    snapshot_.navigation.direction  = nav.direction;
    snapshot_.navigation.position_x = nav.position_x;
    snapshot_.navigation.position_y = nav.position_y;
    snapshot_.navigation.ts         = nav.timestamp;
}

void TelemetryService::collect_system()
{
    TelemetryState::SystemTelemetry sys;
    sys.rssi        = 0;
    sys.heap_free   = static_cast<uint32_t>(esp_get_free_heap_size());
    sys.battery_pct = 100.0f;
    sys.timestamp   = static_cast<uint32_t>(xTaskGetTickCount());

    state_.set_system(sys);

    snapshot_.system.rssi        = sys.rssi;
    snapshot_.system.heap_free   = sys.heap_free;
    snapshot_.system.battery_pct = sys.battery_pct;
    snapshot_.system.ts          = sys.timestamp;
}

void TelemetryService::collect_sensor()
{
    TelemetryState::SensorTelemetry sensor;
    sensor.lidar_distance = 0.0f;
    sensor.humidity       = 0.0f;
    sensor.temperature    = 0.0f;
    sensor.noise_level    = 0.0f;
    sensor.timestamp      = static_cast<uint32_t>(xTaskGetTickCount());

    state_.set_sensor(sensor);

    snapshot_.sensors.lidar_distance = sensor.lidar_distance;
    snapshot_.sensors.humidity       = sensor.humidity;
    snapshot_.sensors.temperature    = sensor.temperature;
    snapshot_.sensors.noise_level    = sensor.noise_level;
    snapshot_.sensors.ts             = sensor.timestamp;
}

void TelemetryService::transmit()
{
    snapshot_.sent_ts = static_cast<uint32_t>(xTaskGetTickCount());

    TaskMessage msg{};
    msg.kind         = msg_kind::TELEMETRY;
    msg.id           = net_msg::TELEMETRY_DATA;
    msg.data         = &snapshot_;
    msg.payload_size = sizeof(TelemetrySnapshot);

    if (!gateway_.post_message(msg)) {
        ESP_LOGW(TAG, "transmit: gateway queue full — dropping snapshot");
    }
}

} // namespace raven
