#include "demo_runtime.hpp"
#include "board_self_test.hpp"
#include "runtime_self_test.hpp"
#include "raven_platform/clock.hpp"
#include "raven_platform/tcp_io.hpp"
#include "raven_gateways/tcp_frame_writer.hpp"
#include "esp_log.h"
#include "lwip/sockets.h"
namespace raven::demo {
using namespace raven::tcp;
static constexpr const char* TAG = "BoardTest";
bool transact(int fd, uint16_t id, const uint8_t* payload, size_t size, Response& response) {
    if (!send_frame(fd, id, payload, size)) return false;
    uint8_t header[6];
    if (!receive_all(fd, header, 6)) return false;
    response.id = uint16_t(header[0]) | uint16_t(header[1]) << 8;
    response.size = wire::u32(header + 2);
    return response.size <= response.payload.size() && receive_all(fd, response.payload.data(), response.size);
}
bool network_self_test(Runtime& runtime) {
    const int client = connect_to("127.0.0.1", CONFIG_RAVEN_RND_PORT);
    if (client < 0) return false;
    Response response;
    uint8_t move[9]{}; wire::put32(move, 20001);
    float angle = 45; std::memcpy(move + 5, &angle, 4);
    bool ok = transact(client, wire::move, move, sizeof(move), response) && response.id == wire::ack;
    ESP_LOGI(TAG, "TCP test admission=%d reply=0x%x", ok, response.id);
    const int listener = listen_at(9091);
    if (listener >= 0) timeouts(listener, 2000);
    const unsigned failures_before = runtime.telemetry.failures();
    runtime.telemetry.use_test_sink(true);
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
             static_cast<unsigned long>(size), runtime.telemetry.frames(), runtime.telemetry.failures());
    ok = ok && std::strstr(payload, "\"arms\"") != nullptr;
    ok = ok && runtime.telemetry.failures() > failures_before;
    { const auto sensors = runtime.sensors.state.current();
      ESP_LOGI(TAG, "TCP test sensor age=%lu", static_cast<unsigned long>(now_ms() - sensors.sample));
      ok &= now_ms() - sensors.sample < 200; }
    runtime.telemetry.use_test_sink(false);
    close_socket(telemetry); close_socket(listener);
    uint8_t cut[5]{3, 0, 0, 0, 7};
    ok = transact(client, wire::cut, cut, sizeof(cut), response) && response.id == wire::ack && ok;
    ESP_LOGI(TAG, "TCP test cut reply=0x%x", response.id);
    close_socket(client);
    return ok;
}
bool command_reconnect_test(Runtime& runtime) {
    // Keep the old socket open: no FIN/RST, no application activity. Test idle,
    // incomplete header and incomplete payload sessions without a radio fault.
    const uint8_t unfinished[]{0x01, 0x11, 9, 0, 0, 0, 1, 0};
    for (const size_t prefix : {size_t(0), size_t(3), sizeof(unfinished)}) {
        const uint32_t id = 21000 + uint32_t(prefix) * 10;
        const int abandoned = connect_to("127.0.0.1", CONFIG_RAVEN_RND_PORT);
        if (abandoned < 0) return false;
        Response reply;
        uint8_t control[9]{}; wire::put32(control, id);
        float angle = 45; std::memcpy(control + 5, &angle, 4);
        bool ok = transact(abandoned, wire::control, control, sizeof(control), reply) && reply.id == wire::ack;
        // ACK precedes execution. Establish Running before checking that the
        // transport takeover leaves this already accepted operation intact.
        const uint64_t execution_deadline = now_ms() + 1000;
        while (runtime.left.state.find(id).status == Status::Unknown && now_ms() < execution_deadline)
            vTaskDelay(pdMS_TO_TICKS(10));
        ok = runtime.left.state.find(id).status == Status::Running && ok;
        if (prefix) ok = send_all(abandoned, unfinished, prefix) && ok;
        const uint64_t started = now_ms();
        const int replacement = connect_to("127.0.0.1", CONFIG_RAVEN_RND_PORT);
        if (replacement >= 0) timeouts(replacement, 2000);
        uint8_t query[9]{}; wire::put32(query, id + 1); wire::put32(query + 5, id);
        ok = replacement >= 0 && transact(replacement, wire::status, query, sizeof(query), reply) &&
             reply.id == wire::snapshot && reply.payload[9] == uint8_t(Status::Running) && ok;
        const uint64_t recovery = now_ms() - started;
        ok = ok && recovery < 2000;
        uint8_t cut[5]{}; wire::put32(cut, id + 2); cut[4] = 1;
        ok = replacement >= 0 && transact(replacement, wire::cut, cut, sizeof(cut), reply) && reply.id == wire::ack && ok;
        ESP_LOGI(TAG, "TCP replacement prefix=%u recovery_ms=%lu ok=%d", unsigned(prefix),
                 static_cast<unsigned long>(recovery), ok);
        close_socket(replacement); close_socket(abandoned);
        if (!ok) return false;
    }
    return true;
}
void test_task(void* context) {
    auto& runtime = *static_cast<Runtime*>(context);
    const bool pure = run_self_tests([](const char* name, bool ok) { ESP_LOGI(TAG, "TEST %s %s", name, ok ? "PASS" : "FAIL"); });
    const uint64_t deadline = now_ms() + 5000;
    while (!runtime.commands.ready() && now_ms() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
    const bool network = runtime.commands.ready() && network_self_test(runtime);
    const bool reconnect = runtime.commands.ready() && command_reconnect_test(runtime);
    ESP_LOGI(TAG, "TEST tcp_session_replacement %s", reconnect ? "PASS" : "FAIL");
    ESP_LOGI(TAG, "stack minimum free bytes: left=%u right=%u activity=%u sensors=%u commands=%u telemetry=%u selftest=%u",
             runtime.left_task.stack_free(), runtime.right_task.stack_free(), runtime.activity_task.stack_free(),
             runtime.sensor_task.stack_free(),
             runtime.commands.stack_free(),
             runtime.telemetry.stack_free(),
             unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    ESP_LOGI(TAG, "TEST tcp_and_slow_telemetry %s", network ? "PASS" : "FAIL");
    ESP_LOGI(TAG, "RND_SELF_TEST %s heap=%u", pure && network && reconnect ? "PASS" : "FAIL", unsigned(esp_get_free_heap_size()));
#ifdef CONFIG_RAVEN_BLE_ENABLED
    // Expose controls only after tests stop issuing commands to these services.
    runtime.ble.start();
#endif
    vTaskDelete(nullptr);
}


void start_board_self_tests(Runtime& runtime) {
    ESP_ERROR_CHECK(xTaskCreate(test_task, "selftest", 32768, &runtime, 2, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
} // namespace raven::demo
