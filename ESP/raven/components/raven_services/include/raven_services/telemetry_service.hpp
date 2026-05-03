#pragma once

#include "raven_services/base_service.hpp"
#include "raven_services/telemetry_messages.hpp"
#include "raven_state/telemetry_state.hpp"
#include "raven_gateways/network_client_gateway.hpp"

#include "freertos/FreeRTOS.h"

namespace raven {

// TelemetryService — periodic telemetry collector and transmitter.
//
// Responsibilities:
//   • Every 100 ms (on_tick) collects Navigation, System, and Sensor data,
//     writes them into TelemetryState, builds a TelemetrySnapshot, and posts
//     it to NetworkClientGateway (msgId = TELEMETRY_DATA) for JSON encoding
//     and transmission.
//   • Reacts to TELEMETRY_ACTIVE (msgId = TELEMETRY_ACTIVE) messages that
//     toggle data collection and transmission on or off.
//
// Real sensor sources are absent in this reference build; placeholder values
// are used instead.
class TelemetryService : public BaseService {
public:
    explicit TelemetryService(TelemetryState& state, NetworkClientGateway& gateway);

protected:
    void on_start() override;
    void handle_message(const TaskMessage& msg) override;
    void on_tick() override;

private:
    void collect_navigation();
    void collect_system();
    void collect_sensor();
    void transmit();

    TelemetryState&       state_;
    NetworkClientGateway& gateway_;
    bool                  active_{true};

    // Snapshot reused across ticks. msg.data points here; the gateway task
    // must process each message before the next tick overwrites the snapshot.
    TelemetrySnapshot snapshot_{};

    static constexpr TickType_t kTickInterval = pdMS_TO_TICKS(10000);
};

} // namespace raven
