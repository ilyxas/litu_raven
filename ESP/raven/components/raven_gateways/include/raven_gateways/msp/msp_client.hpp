#pragma once

#include "raven_gateways/msp/msp_codec.hpp"
#include "raven_platform/ISerialPort.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace raven {

// Owns the request/response discipline for a dedicated MSP UART.
// Only one request may be outstanding at a time.
class MspClient {
public:
    explicit MspClient(ISerialPort& port, size_t max_payload_size = 255);

    bool open();
    void close();
    bool is_open() const;

    bool request(uint8_t command,
                 const void* payload,
                 size_t payload_size,
                 MspFrame& response,
                 uint32_t timeout_ms);

private:
    bool write_all(const std::vector<uint8_t>& frame, uint32_t timeout_ms);

    ISerialPort& port_;
    MspV1Parser parser_;
    std::vector<uint8_t> tx_frame_;
};

} // namespace raven
