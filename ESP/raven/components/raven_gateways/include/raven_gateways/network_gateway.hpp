#pragma once

#include "raven_core/base_task.hpp"
#include "raven_core/task_message.hpp"

#include "raven_gateways/decoder_registry.hpp"
#include "raven_gateways/network_header.hpp"
#include "raven_platform/TcpServer.hpp"
#include "raven_platform/ILink.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include "esp_log.h"

namespace raven {

class NetworkGateway {
public:
    struct Config {
        const char* name;
        uint32_t    stack_size;
        UBaseType_t priority;
        size_t      max_payload_size;
    };

    NetworkGateway(TcpServer& server, const Config& cfg);
    ~NetworkGateway();

    NetworkGateway(const NetworkGateway&) = delete;
    NetworkGateway& operator=(const NetworkGateway&) = delete;

    void start();
    void stop();

    bool register_route(uint16_t msg_id, BaseTask* target);
    bool register_decoder(uint16_t msg_id, const IDecoder* decoder);

private:
    struct RouteEntry {
        uint16_t  msg_id;
        BaseTask* target;
    };

    static void task_entry(void* arg);
    void run();

    bool read_exact(ILink& link, void* dst, size_t len);
    BaseTask* find_route(uint16_t msg_id) const;

    bool dispatch_decoded(BaseTask& target, const DecodedMessageView& decoded);

private:
    TcpServer&        server_;
    Config            cfg_;
    TaskHandle_t      task_handle_;
    DecoderRegistry   decoders_;
    std::vector<RouteEntry> routes_;
    bool              stop_requested_;
};

} // namespace raven