#pragma once

#include "raven_gateways/msp/msp_frame.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace raven {

class MspV1Codec {
public:
    static bool encode_request(uint8_t command,
                               const void* payload,
                               size_t payload_size,
                               std::vector<uint8_t>& output);
};

// Incremental MSP v1 parser. It accepts arbitrary byte chunks, survives frame
// fragmentation, and resynchronises on the next '$' after malformed input.
class MspV1Parser {
public:
    explicit MspV1Parser(size_t max_payload_size = 255);

    void reset();
    bool push(uint8_t byte, MspFrame& output);

private:
    enum class State {
        WaitStart,
        WaitProtocol,
        WaitDirection,
        WaitSize,
        WaitCommand,
        WaitPayload,
        WaitChecksum,
    };

    void restart_from(uint8_t byte);

    size_t max_payload_size_;
    State state_{State::WaitStart};
    MspDirection direction_{MspDirection::FromFlightController};
    uint8_t expected_payload_size_{0};
    uint8_t command_{0};
    uint8_t checksum_{0};
    std::vector<uint8_t> payload_;
};

} // namespace raven
