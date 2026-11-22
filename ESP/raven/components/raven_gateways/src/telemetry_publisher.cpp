#include "raven_gateways/telemetry_publisher.hpp"
#include "raven_gateways/tcp_frame_writer.hpp"
#include "raven_platform/tcp_io.hpp"
#include "raven_platform/clock.hpp"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <cstdio>

namespace raven::demo {
using namespace raven::tcp;
void TelemetryPublisher::start() {
    ESP_ERROR_CHECK(handle_ == nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_ERROR_CHECK(xTaskCreate([](void* ctx) { static_cast<TelemetryPublisher*>(ctx)->run(); },
        "telemetry", 6144, this, 2, &handle_) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void TelemetryPublisher::run() {
    int fd = -1;
    bool test_connection = false;
    for (;;) {
        const bool testing = test_sink_;
        const char* host = testing ? "127.0.0.1" : host_;
        const uint16_t port = testing ? 9091 : port_;
        if (testing != test_connection) { close_socket(fd); fd = -1; test_connection = testing; }
        if (host[0] == '\0') { close_socket(fd); fd = -1; vTaskDelay(pdMS_TO_TICKS(100)); continue; }
        if (fd < 0) fd = connect_to(host, port);
        if (fd < 0) { ++failures_; vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        const auto left = left_.current(), right = right_.current();
        const auto sensors = sensors_.current();
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
            ++failures_; close_socket(fd); fd = -1;
        } else ++frames_;
        // iOS's old server returns plain-text greetings. Drain these separately
        // from the outbound framing so its TCP receive window cannot fill forever.
        uint8_t discard[256];
        if (fd >= 0) recv(fd, discard, sizeof(discard), MSG_DONTWAIT);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
} // namespace raven::demo
