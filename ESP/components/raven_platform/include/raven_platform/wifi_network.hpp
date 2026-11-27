#pragma once
#include "esp_event.h"
#include "esp_netif.h"
#include <atomic>
#include <cstdint>
namespace raven {
// Wi-Fi association is a platform concern. Applications keep executing while
// a station is disconnected; TcpManager obtains the current gateway separately.
class WifiNetwork {
  public:
    void start(bool access_point, const char *ssid, const char *password, unsigned max_stations);
    bool ready() const { return ready_.load(); }
    uint32_t gateway() const { return gateway_.load(); } // network byte order
  private:
    static void event(void *context, esp_event_base_t base, int32_t id, void *data);
    std::atomic<bool> ready_{false};
    std::atomic<uint32_t> gateway_{0};
};
} // namespace raven
