

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "raven_platform/TcpClientLink.hpp"
#include "raven_platform/TcpServer.hpp"

#include "raven_gateways/network_gateway.hpp"
#include "raven_gateways/network_client_gateway.hpp"
#include "raven_events/event_bus.hpp"
#include "raven_state/navigation_state.hpp"
#include "raven_state/telemetry_state.hpp"
#include "raven_services/navigation_service.hpp"
#include "raven_services/pilot_input_service.hpp"
#include "raven_services/telemetry_service.hpp"
#include "raven_activities/navigation_activity.hpp"
#include "raven_core/SystemMonitorTask.hpp"
#include "gateway_bootstrap.hpp"
#include <cstdint>



namespace {
constexpr const char* TAG = "main";

constexpr const char* WIFI_SSID = "iDevice";
constexpr const char* WIFI_PASS = "12345679";

constexpr const char* SERVER_IP = "172.20.10.1";
constexpr uint16_t SERVER_PORT = 8080;

EventGroupHandle_t s_wifi_event_group = nullptr;
constexpr int WIFI_CONNECTED_BIT = BIT0;
constexpr int WIFI_FAIL_BIT = BIT1;

int s_retry_num = 0;
constexpr int MAXIMUM_RETRY = 10;

void wifi_event_handler(void*,
                        esp_event_base_t event_base,
                        int32_t event_id,
                        void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            ++s_retry_num;
            esp_wifi_connect();
            ESP_LOGI(TAG, "retry to connect to AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* event = static_cast<ip_event_got_ip_t*>(event_data);
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

bool wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == nullptr) {
        ESP_LOGE(TAG, "failed to create wifi event group");
        return false;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, nullptr));

    wifi_config_t wifi_config = {};
    strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS, sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.disable_wpa3_compatible_mode = 0;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished");

    const EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to SSID:%s", WIFI_SSID);
        return true;
    }

    ESP_LOGE(TAG, "failed to connect to SSID:%s", WIFI_SSID);
    return false;
}

} // namespace




namespace raven {

extern "C" void app_main() {

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (!wifi_init_sta()) {
        ESP_LOGE(TAG, "wifi init failed");
        return;
    }

    static SystemMonitorTask monitor({
        .name = "system_monitor",
        .stack_size = 4096,
        .priority = 1,
        .period_ticks = pdMS_TO_TICKS(10000)
    });

    // Initialise the ESP-IDF default event loop — required by EventBus.
    raven::EventBus::init();

    // Shared navigation state — single source of truth for position.
    static raven::NavigationState nav_state;

    // Shared telemetry state — stores the latest telemetry snapshots.
    static raven::TelemetryState telemetry_state;

    // Service: stub executor that updates state and signals completion.
    static raven::NavigationService nav_service(nav_state);
    nav_service.start();

    // Service: receives joystick input from the gateway and writes NavigationState.
    static raven::PilotInputService pilot_service(nav_state);
    pilot_service.start();

    // Activity: owns behavior, reacts to directed messages.
    // Receives NavigationState reference to poll joystick data during Manual mode.
    static raven::NavigationActivity nav_activity(nav_service, nav_state);
    nav_activity.start();

    static TcpServer server(8080);

    static NetworkGateway gateway(
        server,
        NetworkGateway::Config{
            .name = "network_gateway",
            .stack_size = 4096,
            .priority = 5,
            .max_payload_size = 4096
        }
    );

    // Outbound gateway: sends telemetry snapshots to the remote server.
    static NetworkClientGateway client_gateway(NetworkClientGateway::Config{
        .name         = "net_client_gw",
        .stack_size   = 4096,
        .priority     = 5,
        .queue_length = 8,
        .host         = SERVER_IP,
        .port         = SERVER_PORT,
    });

    // Service: collects and transmits telemetry every 100 ms.
    static raven::TelemetryService telemetry_service(telemetry_state, client_gateway);
    telemetry_service.start();

    configure_navigation_gateway(gateway, client_gateway, pilot_service, nav_service, nav_activity, telemetry_service);

    gateway.start();
    client_gateway.start();
    monitor.start();
}
}