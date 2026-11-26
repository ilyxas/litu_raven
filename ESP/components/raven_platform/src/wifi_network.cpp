#include "raven_platform/wifi_network.hpp"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <cstdio>
#include <cstring>
namespace raven {
void WifiNetwork::event(void *ctx, esp_event_base_t base, int32_t id, void *data) {
    auto &self = *static_cast<WifiNetwork *>(ctx);
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_START)
        self.ready_ = true;
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STOP)
        self.ready_ = false;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const bool was_ready = self.ready_.exchange(false);
        self.gateway_ = 0;
        if (was_ready)
            ESP_LOGW("Wifi", "station disconnected");
        esp_wifi_connect();
    }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto &ip = *static_cast<ip_event_got_ip_t *>(data);
        self.gateway_ = ip.ip_info.gw.addr;
        self.ready_ = true;
        ESP_LOGI("Wifi", "station ready; gateway=" IPSTR, IP2STR(&ip.ip_info.gw));
    }
}
void WifiNetwork::start(bool ap, const char *ssid, const char *password, unsigned max_stations) {
    auto error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(error);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if (ap)
        esp_netif_create_default_wifi_ap();
    else
        esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event, this));
    wifi_config_t config{};
    if (ap) {
        std::snprintf(reinterpret_cast<char *>(config.ap.ssid), sizeof(config.ap.ssid), "%s", ssid);
        std::snprintf(reinterpret_cast<char *>(config.ap.password), sizeof(config.ap.password), "%s",
                      password);
        config.ap.max_connection = uint8_t(max_stations);
        config.ap.channel = 1;
        config.ap.authmode = std::strlen(password) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    } else {
        std::snprintf(reinterpret_cast<char *>(config.sta.ssid), sizeof(config.sta.ssid), "%s", ssid);
        std::snprintf(reinterpret_cast<char *>(config.sta.password), sizeof(config.sta.password), "%s",
                      password);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(ap ? WIFI_MODE_AP : WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ap ? WIFI_IF_AP : WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    if (!ap)
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}
} // namespace raven
