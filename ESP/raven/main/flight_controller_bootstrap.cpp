#include "flight_controller_bootstrap.hpp"

#include "raven_core/SystemMonitorTask.hpp"
#include "raven_events/event_bus.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"

#ifdef CONFIG_RAVEN_FC_LINK_ENABLED
#include "raven_gateways/betaflight_msp_client.hpp"
#include "raven_gateways/msp/msp_client.hpp"
#include "raven_platform/EspUartPort.hpp"
#include "raven_services/flight_controller_service.hpp"
#include "raven_state/flight_controller_state.hpp"
#endif

namespace raven {
namespace {
constexpr const char* TAG = "ProductBootstrap";
}

void start_product_runtime()
{
    const esp_err_t event_result = EventBus::init();
    if (event_result != ESP_OK) {
        ESP_LOGE(TAG, "event loop init failed: %s", esp_err_to_name(event_result));
        return;
    }

    SystemMonitorTask::Config monitor_config;
    monitor_config.name = "system_monitor";
    monitor_config.stack_size = 4096;
    monitor_config.priority = 1;
    monitor_config.period_ticks = pdMS_TO_TICKS(10000);
    static SystemMonitorTask monitor(monitor_config);

#ifdef CONFIG_RAVEN_FC_LINK_ENABLED
    static EspUartPort fc_uart(EspUartPort::Config{
        static_cast<uart_port_t>(CONFIG_RAVEN_FC_UART_PORT),
        CONFIG_RAVEN_FC_UART_TX_GPIO,
        CONFIG_RAVEN_FC_UART_RX_GPIO,
        CONFIG_RAVEN_FC_MSP_BAUD,
        CONFIG_RAVEN_FC_UART_RX_BUFFER_SIZE,
        0,
    });

    static MspClient msp_client(fc_uart);
    static BetaflightMspClient betaflight_client(msp_client);
    static FlightControllerState flight_controller_state;

    FlightControllerService::Config service_config;
    service_config.poll_interval = pdMS_TO_TICKS(CONFIG_RAVEN_FC_POLL_PERIOD_MS);
    service_config.response_timeout_ms = CONFIG_RAVEN_FC_RESPONSE_TIMEOUT_MS;
    service_config.disconnect_after_failures = CONFIG_RAVEN_FC_DISCONNECT_FAILURES;

    static FlightControllerService flight_controller_service(
        betaflight_client,
        flight_controller_state,
        service_config);

    flight_controller_service.start();
    ESP_LOGI(TAG,
             "FC MSP link enabled on UART%d TX=%d RX=%d baud=%d",
             CONFIG_RAVEN_FC_UART_PORT,
             CONFIG_RAVEN_FC_UART_TX_GPIO,
             CONFIG_RAVEN_FC_UART_RX_GPIO,
             CONFIG_RAVEN_FC_MSP_BAUD);
#else
    ESP_LOGW(TAG, "FC MSP link disabled; configure Raven Flight Controller in menuconfig");
#endif

    monitor.start();
}

} // namespace raven
