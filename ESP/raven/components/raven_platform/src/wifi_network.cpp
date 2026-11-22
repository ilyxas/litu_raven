#include "raven_platform/wifi_network.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>

namespace raven {
static constexpr const char* TAG = "WifiNetwork";
void wifi_event(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) esp_wifi_connect();
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "station address " IPSTR, IP2STR(&event->ip_info.ip));
    }
}
void start_wifi(const char* ssid, const char* password) {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t config{};
    if (ssid[0] != '\0') {
        esp_netif_create_default_wifi_sta();
        std::snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), "%s", ssid);
        std::snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), "%s", password);
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, nullptr));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, nullptr));
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    } else {
        esp_netif_create_default_wifi_ap();
        std::strcpy(reinterpret_cast<char*>(config.ap.ssid), "Raven-RND");
        std::strcpy(reinterpret_cast<char*>(config.ap.password), "raven-rnd");
        config.ap.authmode = WIFI_AUTH_WPA2_PSK; config.ap.max_connection = 2;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
        ESP_LOGI(TAG, "development AP Raven-RND at 192.168.4.1 (virtual actuators only)");
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}
} // namespace raven
