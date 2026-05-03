

#include "raven_gateways/network_gateway.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cassert>
#include <cstring>

extern "C" {
#include "esp_heap_caps.h"
}

namespace raven {

NetworkGateway::NetworkGateway(TcpServer& server, const Config& cfg)
    : server_(server)
    , cfg_(cfg)
    , task_handle_(nullptr)
    , stop_requested_(false)
{
}

NetworkGateway::~NetworkGateway()
{
    stop();
}

void NetworkGateway::start()
{
    assert(task_handle_ == nullptr && "NetworkGateway::start() called more than once");

    BaseType_t result = xTaskCreate(
        &NetworkGateway::task_entry,
        cfg_.name,
        cfg_.stack_size,
        this,
        cfg_.priority,
        &task_handle_
    );

    assert(result == pdPASS && "NetworkGateway: task creation failed");
    ESP_LOGI("NetworkGateway", "Task Created");
}

void NetworkGateway::stop()
{
    ESP_LOGI("NetworkGateway", "Stop Executed");
    stop_requested_ = true;

    server_.stop();

    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
}

bool NetworkGateway::register_route(uint16_t msg_id, BaseTask* target)
{
    if (target == nullptr) {
        return false;
    }

    for (const RouteEntry& e : routes_) {
        if (e.msg_id == msg_id) {
            return false; // duplicate registration
        }
    }

    routes_.push_back({ msg_id, target });
    return true;
}

bool NetworkGateway::register_decoder(uint16_t msg_id, const IDecoder* decoder)
{
    return decoders_.register_decoder(msg_id, decoder);
}

// static
void NetworkGateway::task_entry(void* arg)
{
    static_cast<NetworkGateway*>(arg)->run();
}

bool NetworkGateway::read_exact(ILink& link, void* dst, size_t len)
{
    uint8_t* out = static_cast<uint8_t*>(dst);
    size_t total = 0;

    while (total < len) {
        const ssize_t r = link.read(out + total, len - total);
        if (r <= 0) {
            return false;
        }
        total += static_cast<size_t>(r);
    }

    return true;
}

BaseTask* NetworkGateway::find_route(uint16_t msg_id) const
{
    for (const RouteEntry& e : routes_) {
        if (e.msg_id == msg_id) {
            return e.target;
        }
    }
    return nullptr;
}

bool NetworkGateway::dispatch_decoded(BaseTask& target, const DecodedMessageView& decoded)
{
    TaskMessage msg {};
    msg.kind = decoded.kind;
    msg.id = decoded.id;
    msg.payload_size = decoded.payload_size;
    msg.data = const_cast<void*>(decoded.payload); // borrowed only for enqueue-time copy

    return target.post_message(msg);
}

void NetworkGateway::run()
{
    ESP_LOGI("NetworkGateway", "Run started...");
    std::vector<uint8_t> rx_buffer(sizeof(NetworkHeader) + cfg_.max_payload_size);

    if (!server_.isRunning()) {
        const bool started = server_.start();
        if (!started) {
            vTaskDelete(nullptr);
            return;
        }
        ESP_LOGI("NetworkGateway", "server listening on 8080");
    }

    while (!stop_requested_) {
        std::unique_ptr<ILink> link = server_.acceptConnection();
        if (!link) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        while (!stop_requested_ && link->isOpen()) {
            NetworkHeader header {};
            if (!read_exact(*link, &header, sizeof(header))) {
                ESP_LOGI("NetworkGateway", "read_exact Failed");
                link->close();
                break;
            }

            if (header.payload_size > cfg_.max_payload_size) {
                ESP_LOGI("NetworkGateway", "payload_size Failed %u", header.payload_size);
                link->close();
                break;
            }

            uint8_t* payload = nullptr;
            if (header.payload_size > 0) {
                payload = rx_buffer.data() + sizeof(NetworkHeader);

                if (!read_exact(*link, payload, header.payload_size)) {
                    ESP_LOGI("NetworkGateway", "read exact payload_size Failed");
                    link->close();
                    break;
                }
            }

            const IDecoder* decoder = decoders_.find(header.msg_id);
            if (decoder == nullptr) {
                // unknown msg_id -> ignore this packet
                ESP_LOGI("NetworkGateway", "Unknown msg id");
                continue;
            }

            DecodedMessageView decoded {};
            const bool decoded_ok = decoder->decode(header, payload, decoded);
            if (!decoded_ok) {
                ESP_LOGI("NetworkGateway", "Paylod is not decoded");
                continue;
            }

            BaseTask* target = find_route(decoded.id);
            if (target == nullptr) {
                // known/decoded packet, but no route configured
                ESP_LOGI("NetworkGateway", "No route exist for message id %u", decoded.id);
                continue;
            }

            const bool dispatched = dispatch_decoded(*target, decoded);
            if (!dispatched) {
                // queue full / allocation failed
                // For now just drop the message.
                // Later you can add logging or backpressure.
                continue;
            }
        }
    }
    ESP_LOGI("NetworkGateway", "Run ended...");
    vTaskDelete(nullptr);
}

} // namespace raven