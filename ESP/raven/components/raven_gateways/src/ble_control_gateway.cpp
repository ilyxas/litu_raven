#include "raven_gateways/ble_control_gateway.hpp"
#include "raven_platform/clock.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

namespace raven::demo {
void BleControlGateway::start() {
    ESP_ERROR_CHECK(handle_ == nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    server_.start("Raven-BLE");
    ESP_ERROR_CHECK(xTaskCreate([](void *ctx) { static_cast<BleControlGateway *>(ctx)->run(); },
                                "ble_gateway", 6144, this, 3, &handle_) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}
bool BleControlGateway::emit(void *ctx, const char *bytes, size_t size) {
    auto &self = *static_cast<BleControlGateway *>(ctx);
    return self.server_.send(self.session_, bytes, size);
}
void BleControlGateway::run() {
    uint64_t next_readout = 0, next_diagnostic = 10000;
    char last_temperature[32]{};
    for (;;) {
        const auto session = server_.session();
        if (session != session_) {
            session_ = session;
            protocol_.reset();
            next_readout = 0;
            last_temperature[0] = '\0';
        }
        BleSerialServer::Packet packet;
        // Finite input batches; readouts and other tasks cannot be starved.
        for (unsigned i = 0; i < 8 && server_.receive(packet); ++i) {
            if (packet.session != session_ || !server_.ready())
                continue;
            if (!protocol_.feed(packet.data.data(), packet.size, emit, this)) {
                server_.disconnect(session_);
                break;
            }
        }
        if (!protocol_.watching()) last_temperature[0] = '\0';
        if (server_.ready() && server_.tx_idle() && protocol_.watching() && now_ms() >= next_readout) {
            bool ok = protocol_.readouts(emit, this);
            char temperature[32];
            const int size = std::snprintf(temperature, sizeof(temperature), "G2:%.1f\n",
                                           double(sensors_.current().temperature));
            if (ok && std::strcmp(last_temperature, temperature)) {
                ok = emit(this, temperature, size_t(size));
                if (ok) std::strcpy(last_temperature, temperature);
            }
            if (!ok)
                server_.disconnect(session_);
            next_readout = now_ms() + 500;
        }
        if (now_ms() >= next_diagnostic) {
            ESP_LOGI("BleGateway", "notifications_ready=%u stack_free gateway=%u host=%u", unsigned(server_.ready()),
                     unsigned(uxTaskGetStackHighWaterMark(nullptr)), server_.host_stack_free());
            next_diagnostic = now_ms() + 10000;
        }
        vTaskDelay(pdMS_TO_TICKS(10) ? pdMS_TO_TICKS(10) : 1);
    }
}
} // namespace raven::demo
