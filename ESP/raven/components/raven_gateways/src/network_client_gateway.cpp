#include "raven_gateways/network_client_gateway.hpp"

#include "esp_log.h"

#include <cstdint>

static const char* TAG = "NetworkClientGateway";

namespace raven {

NetworkClientGateway::NetworkClientGateway(const Config& cfg)
    : BaseTask({ cfg.name, cfg.stack_size, cfg.priority, cfg.queue_length })
    , link_(cfg.host, cfg.port)
{
}

bool NetworkClientGateway::register_encoder(uint16_t kind, uint16_t id, const IEncoder* encoder)
{
    return encoders_.register_encoder(kind, id, encoder);
}

void NetworkClientGateway::handle_message(const TaskMessage& msg)
{
    const IEncoder* encoder = encoders_.find(msg.kind, msg.id);
    if (encoder == nullptr) {
        ESP_LOGW(TAG, "No encoder registered for kind=%u id=%u — dropping message", msg.kind, msg.id);
        return;
    }

    EncodedFrame frame {};
    if (!encoder->encode(msg, frame)) {
        ESP_LOGW(TAG, "Encoder failed for kind=%u id=%u — dropping message", msg.kind, msg.id);
        return;
    }

    if (!ensure_connected()) {
        ESP_LOGW(TAG, "Could not connect to remote server — dropping message");
        return;
    }

    const ssize_t written = link_.write(frame.data, frame.size);
    if (written != static_cast<ssize_t>(frame.size)) {
        ESP_LOGW(TAG, "Write failed — closing link");
        link_.close();
    }
}

bool NetworkClientGateway::ensure_connected()
{
    if (link_.isOpen()) {
        return true;
    }

    const bool ok = link_.open();
    if (!ok) {
        ESP_LOGW(TAG, "Failed to open TCP link");
    }
    return ok;
}

} // namespace raven
