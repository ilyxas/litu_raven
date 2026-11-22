#include "raven_gateways/command_server.hpp"
#include "raven_gateways/frame_reader.hpp"
#include "raven_gateways/tcp_frame_writer.hpp"
#include "raven_platform/tcp_io.hpp"
#include "lwip/sockets.h"
#include "esp_log.h"
#include <cerrno>

namespace raven::demo {
using namespace raven::tcp;
static constexpr const char* TAG = "CommandServer";
void CommandServer::start() {
    ESP_ERROR_CHECK(handle_ == nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    ESP_ERROR_CHECK(xTaskCreate([](void* ctx) { static_cast<CommandServer*>(ctx)->run(); },
        "tcp_commands", 6144, this, 3, &handle_) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
void CommandServer::run() {
    const int server = listen_at(port_);
    if (server < 0) { ESP_LOGE(TAG, "command listener failed"); vTaskDelete(nullptr); return; }
    ready_ = true;
    ESP_LOGI(TAG, "TCP_NETWORK_READY port=%d", port_);
    int client = -1;
    FrameReader reader(protocol_);
    unsigned batch = 0;
    for (;;) {
        fd_set reads; FD_ZERO(&reads); FD_SET(server, &reads);
        if (client >= 0) FD_SET(client, &reads);
        timeval wait{0, 100000};
        const int ready = select((client > server ? client : server) + 1, &reads, nullptr, nullptr, &wait);
        if (ready < 0) { vTaskDelay(1); continue; }
        if (ready == 0) { batch = 0; continue; }
        // Newest controller wins. An abandoned socket never monopolizes accept.
        if (FD_ISSET(server, &reads)) {
            const int replacement = accept(server, nullptr, nullptr);
            if (replacement >= 0) {
                if (command_options(replacement)) {
                    if (client >= 0) close(client); // SO_LINGER=0; no blocking graceful shutdown.
                    client = replacement;
                    reader.reset();
                } else {
                    ESP_LOGE(TAG, "command socket configuration failed: %d", errno);
                    close(replacement);
                }
            }
        } else if (client >= 0 && FD_ISSET(client, &reads)) {
            uint8_t bytes[wire::max_payload];
            const int count = recv(client, bytes, reader.bytes_needed(), 0);
            const bool retry = count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
            const bool received = count > 0 && reader.feed(bytes, static_cast<size_t>(count),
                [](void* context, const Response& reply) {
                    return send_frame(*static_cast<int*>(context), reply.id, reply.payload.data(), reply.size);
                }, &client);
            if (!retry && !received) { close(client); client = -1; reader.reset(); }
        }
        if (++batch == 8) { vTaskDelay(1); batch = 0; }
    }
}
} // namespace raven::demo
