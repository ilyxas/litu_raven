#pragma once

#include "raven_gateways/betaflight_msp_client.hpp"
#include "raven_services/base_service.hpp"
#include "raven_state/flight_controller_state.hpp"

#include "freertos/FreeRTOS.h"

#include <cstdint>

namespace raven {

// Owns the flight-controller polling policy and the MSP client execution
// context. The gateway performs protocol transactions; this service decides
// when they happen and records their semantic result in shared state.
class FlightControllerService final : public BaseService {
public:
    struct Config {
        const char* task_name{"fc_service"};
        uint32_t stack_size{4096};
        UBaseType_t priority{6};
        UBaseType_t queue_length{4};
        TickType_t poll_interval{pdMS_TO_TICKS(1000)};
        uint32_t response_timeout_ms{250};
        uint32_t disconnect_after_failures{3};
    };

    FlightControllerService(IBetaflightClient& client,
                            FlightControllerState& state,
                            const Config& config);

protected:
    void on_start() override;
    void handle_message(const TaskMessage& message) override;
    void on_tick() override;

private:
    static uint32_t now_ms();
    void poll_link();
    void handle_failure();
    void publish_link_event(int32_t event_id);

    IBetaflightClient& client_;
    FlightControllerState& state_;
    Config config_;
};

} // namespace raven
