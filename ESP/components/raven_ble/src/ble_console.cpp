#include "raven_platform/ble_console.hpp"
#include "esp_check.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
namespace raven {
BleConsoleTx::BleConsoleTx(BleSerialServer &serial, diagnostics::Journal &journal)
    : serial_(serial), journal_(journal) {
    const bool a =
        inbox.subscribe(services::console_line_id, sizeof(services::ConsoleLine), runtime::Policy::Ordered);
    const bool b =
        inbox.subscribe(services::console_watch_id, sizeof(services::ConsoleWatch), runtime::Policy::Ordered);
    assert(a && b);
    (void)a;
    (void)b;
}
bool BleConsoleTx::send(uint32_t generation, const char *bytes, size_t size) {
    if (generation != serial_.session() || !serial_.ready())
        return false;
    char line[64]{};
    const auto count = std::min<size_t>(size, 56);
    std::memcpy(line, "D0:", 3);
    for (size_t i = 0; i < count; ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        line[3 + i] = c < 32 || c == 127 ? ' ' : bytes[i];
    }
    line[3 + count] = '\n';
    return serial_.send(generation, line, count + 4);
}
void BleConsoleTx::on_message(const runtime::Message &message, uint64_t) {
    synchronize();
    if (message.id == services::console_line_id) {
        services::ConsoleLine value{};
        if (!message.read(value))
            return;
        if (value.generation != serial_.session() || !serial_.ready())
            return;
        if (!send(value.generation, value.text, std::min<size_t>(value.size, sizeof(value.text))))
            serial_.disconnect(value.generation);
    } else if (message.id == services::console_watch_id) {
        services::ConsoleWatch value{};
        if (!message.read(value) || value.generation != serial_.session() || !serial_.ready())
            return;
        watch_ = value.enabled;
        cursor_ = journal_.latest() + 1;
        const char *reply = watch_ ? "watch on" : "watch off";
        if (!send(value.generation, reply, std::strlen(reply)))
            serial_.disconnect(value.generation);
    }
}
void BleConsoleTx::synchronize() {
    if (!serial_.ready())
        return;
    if (generation_ != serial_.session()) {
        generation_ = serial_.session();
        watch_ = true;
        cursor_ = journal_.latest() + 1;
        constexpr char hello[] = "Raven service console; type help";
        send(generation_, hello, sizeof(hello) - 1);
    }
}
void BleConsoleTx::on_tick(uint64_t now) {
    synchronize();
    if (!serial_.ready())
        return;
    if (!watch_ || inbox.pending() || !serial_.tx_idle() || now < next_event_)
        return;
    next_event_ = now + 250;
    if (cursor_ < journal_.oldest()) {
        char text[56]{};
        std::snprintf(text, sizeof(text), "log gap: %llu events",
                      (unsigned long long)(journal_.oldest() - cursor_));
        if (send(generation_, text, std::strlen(text)))
            cursor_ = journal_.oldest();
        return;
    }
    diagnostics::Record record{};
    if (!journal_.read(cursor_, record))
        return;
    char text[56]{};
    std::snprintf(text, sizeof(text), "%012llX e=%u %.28s", (unsigned long long)record.device,
                  unsigned(record.code), record.text);
    if (send(generation_, text, std::strlen(text)))
        ++cursor_;
}
void BleConsoleRx::start() {
    const auto result = xTaskCreate([](void *p) { static_cast<BleConsoleRx *>(p)->run(); }, "ble_rx", 3072,
                                    this, 3, &handle_);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void BleConsoleRx::run() {
    BleSerialServer::Packet packet;
    for (;;) {
        if (serial_.receive(packet) && serial_.ready() && packet.session == serial_.session()) {
            parser_.feed(
                packet.session, packet.data.data(), packet.size,
                [](void *ctx, const services::ConsoleRequest &request) {
                    auto &self = *static_cast<BleConsoleRx *>(ctx);
                    const auto result = self.dispatcher_.deliver(
                        runtime::destination(self.destination_),
                        runtime::Message::make(services::console_command_id, request.request, request));
                    if (!result.complete())
                        self.serial_.disconnect(request.generation);
                },
                this);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace raven
