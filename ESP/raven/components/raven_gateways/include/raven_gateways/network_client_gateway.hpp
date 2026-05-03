#pragma once

#include "raven_core/base_task.hpp"
#include "raven_core/task_message.hpp"

#include "raven_gateways/encoder_registry.hpp"
#include "raven_platform/TcpClientLink.hpp"

#include <cstddef>
#include <cstdint>

namespace raven {

// NetworkClientGateway — outbound TCP client gateway.
//
// Mirrors NetworkGateway (inbound server-side) in the reverse direction.
//
// NetworkGateway:        TCP server → decode wire frame → route to internal task
// NetworkClientGateway:  internal task message → encode → send over TCP client
//
// Derives from BaseTask so that all socket I/O and encoding happen inside the
// owner task context, driven by the inbound message queue.
//
// Typical usage:
//   NetworkClientGateway gw({ "net_out", 4096, 5, 8, "192.168.1.10", 9000 });
//   static RawEncoder enc;
//   gw.register_encoder(msg_kind::MY_KIND, net_msg::MY_ID, &enc);
//   gw.start();
//   gw.post_message({ msg_kind::MY_KIND, net_msg::MY_ID, payload_ptr, payload_size });
class NetworkClientGateway : public BaseTask {
public:
    struct Config {
        const char*  name;          // FreeRTOS task name
        uint32_t     stack_size;    // task stack in bytes
        UBaseType_t  priority;      // FreeRTOS task priority
        UBaseType_t  queue_length;  // inbound queue capacity (messages)
        const char*  host;          // remote server host (IP or hostname)
        uint16_t     port;          // remote server port
    };

    explicit NetworkClientGateway(const Config& cfg);
    ~NetworkClientGateway() override = default;

    NetworkClientGateway(const NetworkClientGateway&)            = delete;
    NetworkClientGateway& operator=(const NetworkClientGateway&) = delete;

    // Register an encoder for the given (kind, id) key.
    // Must be called before start().
    // Encoder lifetime must exceed the gateway task lifetime.
    bool register_encoder(uint16_t kind, uint16_t id, const IEncoder* encoder);

protected:
    // Called for each message dequeued from the inbound queue.
    // Looks up the registered encoder, serialises the message into a network
    // frame, and sends it to the remote server via TcpClientLink.
    void handle_message(const TaskMessage& msg) override;

private:
    // Ensure the TCP link is open; attempts to connect if not already connected.
    // Returns true if the link is ready for writing.
    bool ensure_connected();

    TcpClientLink   link_;
    EncoderRegistry encoders_;
};

} // namespace raven
