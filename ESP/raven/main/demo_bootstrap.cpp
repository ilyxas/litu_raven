#include "demo_runtime.hpp"
#include "raven_platform/wifi_network.hpp"
#include "raven_core/SystemMonitorTask.hpp"
#include "board_self_test.hpp"
#include "esp_check.h"
namespace raven::demo {
void start_demo_runtime() {
    static Runtime runtime;
    start_wifi(CONFIG_RAVEN_RND_WIFI_SSID, CONFIG_RAVEN_RND_WIFI_PASSWORD);
    ESP_ERROR_CHECK(runtime.dispatcher.bind(0, runtime.left.inbox) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runtime.dispatcher.bind(1, runtime.right.inbox) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runtime.dispatcher.bind(2, runtime.activity.inbox) ? ESP_OK : ESP_FAIL);
    runtime.left_task.start();
    runtime.right_task.start();
    runtime.activity_task.start();
    runtime.sensor_task.start();
    runtime.commands.start();
    runtime.telemetry.start();
    static SystemMonitorTask monitor({"system_monitor", 4096, 1, pdMS_TO_TICKS(10000)});
    monitor.start();
#ifdef CONFIG_RAVEN_RND_SELF_TEST
    start_board_self_tests(runtime);
#elif defined(CONFIG_RAVEN_BLE_ENABLED)
    runtime.ble.start();
#endif
}
} // namespace raven::demo
