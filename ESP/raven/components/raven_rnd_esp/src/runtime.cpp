#include "raven_rnd_esp/runtime.hpp"
#include "raven_rnd/protocol.hpp"
#include "raven_rnd/self_test.hpp"
#include "raven_core/SystemMonitorTask.hpp"
#include "raven_events/event_bus.hpp"

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#include <atomic>
#include <cerrno>
#include <cstdio>

namespace raven {
namespace {
using namespace rnd;
constexpr const char* TAG = "RavenRND";
uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }

class OwnerTask {
public:
    OwnerTask(Component& component, const char* name, UBaseType_t priority)
        : component_(component), executor_(component, 10), name_(name), priority_(priority) {}
    void start() {
        component_.inbox.set_wake([](void* ctx) {
            auto& self = *static_cast<OwnerTask*>(ctx);
            if (self.handle_) xTaskNotifyGive(self.handle_);
        }, this);
        const auto result = xTaskCreate([](void* ctx) { static_cast<OwnerTask*>(ctx)->run(); },
                                       name_, 4096, this, priority_, &handle_);
        ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    }
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }
private:
    void run() {
        unsigned batch = 0;
        for (;;) {
            executor_.step(now_ms());
            if (component_.inbox.pending() && ++batch < 8) continue;
            if (component_.inbox.pending()) vTaskDelay(1); // Give idle/other owners CPU under sustained ingress.
            else ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10) ? pdMS_TO_TICKS(10) : 1);
            batch = 0;
        }
    }
    Component& component_;
    Executor executor_;
    const char* name_;
    UBaseType_t priority_;
    TaskHandle_t handle_{};
};

struct Sensors { uint64_t sample{}; float temperature{}; };
struct Runtime {
    SimArmDriver left_driver, right_driver;
    ArmService left{left_driver}, right{right_driver};
    Dispatcher dispatcher;
    DualArmActivity activity{dispatcher, left.state, right.state};
    Protocol protocol{dispatcher, left.state, right.state, activity.state};
    OwnerTask left_task{left, "left_arm", 5}, right_task{right, "right_arm", 5}, activity_task{activity, "activity", 4};
    std::mutex sensor_mutex;
    Sensors sensors;
    std::atomic<bool> server_ready{false}, test_telemetry{false};
    std::atomic<unsigned> telemetry_frames{0}, telemetry_failures{0};
    TaskHandle_t sensor_handle{}, command_handle{}, telemetry_handle{};
};
Runtime runtime;

void close_socket(int fd) { if (fd >= 0) { shutdown(fd, SHUT_RDWR); close(fd); } }
void timeouts(int fd, int milliseconds) {
    const timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    const int no_delay = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
}
bool send_all(int fd, const void* buffer, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    const uint64_t deadline = now_ms() + 500;
    while (size && now_ms() < deadline) {
        const int sent = send(fd, bytes, size, 0);
        if (sent <= 0) return false;
        bytes += sent; size -= static_cast<size_t>(sent);
    }
    return size == 0;
}
[[maybe_unused]] bool receive_all(int fd, void* buffer, size_t size) {
    auto* bytes = static_cast<uint8_t*>(buffer);
    const uint64_t deadline = now_ms() + 2000;
    while (size && now_ms() < deadline) {
        const int count = recv(fd, bytes, size, 0);
        if (count <= 0) return false;
        bytes += count; size -= static_cast<size_t>(count);
    }
    return size == 0;
}
bool send_frame(int fd, uint16_t id, const uint8_t* payload, size_t size) {
    if (size > 1024) return false;
    uint8_t frame[1030]{static_cast<uint8_t>(id), static_cast<uint8_t>(id >> 8)};
    wire::put32(frame + 2, static_cast<uint32_t>(size));
    std::memcpy(frame + 6, payload, size);
    // One frame/write avoids the Nagle + delayed-ACK interaction between a tiny
    // header and its payload. Partial writes are still completed by send_all.
    return send_all(fd, frame, size + 6);
}
int listen_at(uint16_t port) {
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_ANY); address.sin_port = htons(port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(fd, 1)) {
        close_socket(fd); return -1;
    }
    return fd;
}
int connect_to(const char* host, uint16_t port) {
    // Numeric IPv4 only: bounded connect, no blocking DNS in the telemetry worker.
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) return -1;
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    const int result = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result && errno != EINPROGRESS) { close_socket(fd); return -1; }
    fd_set writes; FD_ZERO(&writes); FD_SET(fd, &writes);
    timeval wait{0, 300000};
    int error = 0; socklen_t length = sizeof(error);
    if (select(fd + 1, nullptr, &writes, nullptr, &wait) <= 0 ||
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) || error) { close_socket(fd); return -1; }
    fcntl(fd, F_SETFL, flags);
    timeouts(fd, 200);
    return fd;
}
void command_task(void*) {
    const int server = listen_at(CONFIG_RAVEN_RND_PORT);
    if (server < 0) { ESP_LOGE(TAG, "command listener failed"); vTaskDelete(nullptr); return; }
    runtime.server_ready = true;
    ESP_LOGI(TAG, "RND_NETWORK_READY port=%d", CONFIG_RAVEN_RND_PORT);
    for (;;) {
        int client = accept(server, nullptr, nullptr);
        if (client < 0) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        timeouts(client, 200);
        FrameReader reader(runtime.protocol);
        uint8_t bytes[256];
        for (;;) {
            const int count = recv(client, bytes, sizeof(bytes), 0);
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            if (count <= 0) break;
            if (!reader.feed(bytes, static_cast<size_t>(count), [](void* context, const Response& reply) {
                return send_frame(*static_cast<int*>(context), reply.id, reply.payload.data(), reply.size);
            }, &client)) break;
            vTaskDelay(1); // Bound ingress CPU use under a continuously readable socket.
        }
        close_socket(client);
    }
}
void sensor_task(void*) {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(runtime.sensor_mutex);
            runtime.sensors = {now_ms(), 22.0f + float((now_ms() / 100) % 10) / 10.0f};
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
void telemetry_task(void*) {
    int fd = -1;
    bool test_connection = false;
    for (;;) {
        const bool testing = runtime.test_telemetry;
        const char* host = testing ? "127.0.0.1" : CONFIG_RAVEN_RND_TELEMETRY_HOST;
        const uint16_t port = testing ? 9091 : CONFIG_RAVEN_RND_TELEMETRY_PORT;
        if (testing != test_connection) { close_socket(fd); fd = -1; test_connection = testing; }
        if (host[0] == '\0') { close_socket(fd); fd = -1; vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (fd < 0) fd = connect_to(host, port);
        if (fd < 0) { ++runtime.telemetry_failures; vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        const auto left = runtime.left.state.current(), right = runtime.right.state.current();
        Sensors sensors;
        { std::lock_guard<std::mutex> lock(runtime.sensor_mutex); sensors = runtime.sensors; }
        char json[1024];
        // Preserve the legacy iOS telemetry object's required fields; new readers
        // additionally observe arms. These sensor/navigation values are mock data.
        const int size = std::snprintf(json, sizeof(json),
            "{\"sent_ts\":%lu,\"navigation\":{\"speed\":0,\"direction\":0,\"position_x\":0,\"position_y\":0,\"ts\":%lu},"
            "\"system\":{\"rssi\":0,\"heap_free\":%u,\"battery_pct\":100,\"ts\":%lu},"
            "\"sensors\":{\"lidar_distance\":0,\"humidity\":0,\"temperature\":%.1f,\"noise_level\":0,\"ts\":%lu},"
            "\"arms\":[{\"operation\":%lu,\"status\":%u,\"position\":%.2f,\"target\":%.2f},"
            "{\"operation\":%lu,\"status\":%u,\"position\":%.2f,\"target\":%.2f}]}",
            static_cast<unsigned long>(now_ms()), static_cast<unsigned long>(sensors.sample),
            unsigned(esp_get_free_heap_size()), static_cast<unsigned long>(sensors.sample),
            double(sensors.temperature), static_cast<unsigned long>(sensors.sample),
            static_cast<unsigned long>(left.operation), unsigned(left.status), double(left.position), double(left.target),
            static_cast<unsigned long>(right.operation), unsigned(right.status), double(right.position), double(right.target));
        if (size <= 0 || size >= int(sizeof(json)) ||
            !send_frame(fd, 0x3001, reinterpret_cast<const uint8_t*>(json), static_cast<size_t>(size))) {
            ++runtime.telemetry_failures; close_socket(fd); fd = -1;
        } else ++runtime.telemetry_frames;
        // iOS's old server returns plain-text greetings. Drain these separately
        // from the outbound framing so its TCP receive window cannot fill forever.
        uint8_t discard[256];
        if (fd >= 0) recv(fd, discard, sizeof(discard), MSG_DONTWAIT);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#ifdef CONFIG_RAVEN_RND_SELF_TEST
bool transact(int fd, uint16_t id, const uint8_t* payload, size_t size, Response& response) {
    if (!send_frame(fd, id, payload, size)) return false;
    uint8_t header[6];
    if (!receive_all(fd, header, 6)) return false;
    response.id = uint16_t(header[0]) | uint16_t(header[1]) << 8;
    response.size = wire::u32(header + 2);
    return response.size <= response.payload.size() && receive_all(fd, response.payload.data(), response.size);
}
bool network_self_test() {
    const int client = connect_to("127.0.0.1", CONFIG_RAVEN_RND_PORT);
    if (client < 0) return false;
    Response response;
    uint8_t move[9]{}; wire::put32(move, 20001);
    float angle = 45; std::memcpy(move + 5, &angle, 4);
    bool ok = transact(client, wire::move, move, sizeof(move), response) && response.id == wire::ack;
    ESP_LOGI(TAG, "TCP test admission=%d reply=0x%x", ok, response.id);
    const int listener = listen_at(9091);
    if (listener >= 0) timeouts(listener, 2000);
    const unsigned failures_before = runtime.telemetry_failures;
    runtime.test_telemetry = true;
    const int telemetry = listener < 0 ? -1 : accept(listener, nullptr, nullptr);
    if (telemetry >= 0) timeouts(telemetry, 2000);
    // Stop reading until the actual TCP send/receive windows fill. Motion and
    // sensors must continue while telemetry hits its socket timeout and reconnects.
    vTaskDelay(pdMS_TO_TICKS(6000));
    uint8_t query[9]{}; wire::put32(query, 20002); wire::put32(query + 5, 20001);
    ok = transact(client, wire::status, query, sizeof(query), response) && ok;
    ESP_LOGI(TAG, "TCP test status reply=0x%x state=%u position=%.2f", response.id,
             unsigned(response.payload[9]), double(runtime.left.state.current().position));
    ok = ok && response.id == wire::snapshot && response.payload[9] == uint8_t(Status::Succeeded);
    uint8_t header[6]{}; char payload[1024]{};
    const bool got_header = telemetry >= 0 && receive_all(telemetry, header, 6);
    const uint32_t size = wire::u32(header + 2);
    ok = got_header && size < sizeof(payload) && receive_all(telemetry, payload, size) && ok;
    ESP_LOGI(TAG, "TCP test telemetry header=%d size=%lu frames=%u failures=%u", got_header,
             static_cast<unsigned long>(size), runtime.telemetry_frames.load(), runtime.telemetry_failures.load());
    ok = ok && std::strstr(payload, "\"arms\"") != nullptr;
    ok = ok && runtime.telemetry_failures > failures_before;
    { std::lock_guard<std::mutex> lock(runtime.sensor_mutex);
      ESP_LOGI(TAG, "TCP test sensor age=%lu", static_cast<unsigned long>(now_ms() - runtime.sensors.sample));
      ok &= now_ms() - runtime.sensors.sample < 200; }
    runtime.test_telemetry = false;
    close_socket(telemetry); close_socket(listener);
    uint8_t cut[5]{3, 0, 0, 0, 7};
    ok = transact(client, wire::cut, cut, sizeof(cut), response) && response.id == wire::ack && ok;
    ESP_LOGI(TAG, "TCP test cut reply=0x%x", response.id);
    close_socket(client);
    return ok;
}
void test_task(void*) {
    const bool pure = run_self_tests([](const char* name, bool ok) { ESP_LOGI(TAG, "TEST %s %s", name, ok ? "PASS" : "FAIL"); });
    const uint64_t deadline = now_ms() + 5000;
    while (!runtime.server_ready && now_ms() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
    const bool network = runtime.server_ready && network_self_test();
    ESP_LOGI(TAG, "stack minimum free bytes: left=%u right=%u activity=%u sensors=%u commands=%u telemetry=%u selftest=%u",
             runtime.left_task.stack_free(), runtime.right_task.stack_free(), runtime.activity_task.stack_free(),
             unsigned(uxTaskGetStackHighWaterMark(runtime.sensor_handle)),
             unsigned(uxTaskGetStackHighWaterMark(runtime.command_handle)),
             unsigned(uxTaskGetStackHighWaterMark(runtime.telemetry_handle)),
             unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    ESP_LOGI(TAG, "TEST tcp_and_slow_telemetry %s", network ? "PASS" : "FAIL");
    ESP_LOGI(TAG, "RND_SELF_TEST %s heap=%u", pure && network ? "PASS" : "FAIL", unsigned(esp_get_free_heap_size()));
    vTaskDelete(nullptr);
}
#endif

void wifi_event(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) esp_wifi_connect();
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(TAG, "station address " IPSTR, IP2STR(&event->ip_info.ip));
    }
}
void start_wifi() {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(EventBus::init());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t config{};
    if (CONFIG_RAVEN_RND_WIFI_SSID[0] != '\0') {
        esp_netif_create_default_wifi_sta();
        std::snprintf(reinterpret_cast<char*>(config.sta.ssid), sizeof(config.sta.ssid), "%s", CONFIG_RAVEN_RND_WIFI_SSID);
        std::snprintf(reinterpret_cast<char*>(config.sta.password), sizeof(config.sta.password), "%s", CONFIG_RAVEN_RND_WIFI_PASSWORD);
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
TaskHandle_t task(TaskFunction_t entry, const char* name, uint32_t stack, UBaseType_t priority) {
    TaskHandle_t handle{};
    const auto result = xTaskCreate(entry, name, stack, nullptr, priority, &handle);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    return handle;
}
void start_prototype() {
    start_wifi();
    ESP_ERROR_CHECK(runtime.dispatcher.bind(0, runtime.left.inbox) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runtime.dispatcher.bind(1, runtime.right.inbox) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(runtime.dispatcher.bind(2, runtime.activity.inbox) ? ESP_OK : ESP_FAIL);
    runtime.left_task.start(); runtime.right_task.start(); runtime.activity_task.start();
    runtime.sensor_handle = task(sensor_task, "sensors", 3072, 3);
    runtime.command_handle = task(command_task, "tcp_commands", 6144, 3);
    runtime.telemetry_handle = task(telemetry_task, "telemetry", 6144, 2);
    static SystemMonitorTask monitor({"system_monitor", 4096, 1, pdMS_TO_TICKS(10000)});
    monitor.start();
#ifdef CONFIG_RAVEN_RND_SELF_TEST
    task(test_task, "rnd_selftest", 32768, 2);
#endif
}
} // namespace
void start_rnd_runtime() { start_prototype(); }
} // namespace raven
