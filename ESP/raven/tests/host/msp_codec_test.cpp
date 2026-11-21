#include "raven_gateways/msp/msp_codec.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

std::vector<uint8_t> response(uint8_t command,
                              const std::vector<uint8_t>& payload,
                              bool valid_checksum = true)
{
    std::vector<uint8_t> frame{'$', 'M', '>', static_cast<uint8_t>(payload.size()), command};
    uint8_t checksum = static_cast<uint8_t>(payload.size()) ^ command;
    for (const uint8_t byte : payload) {
        frame.push_back(byte);
        checksum ^= byte;
    }
    frame.push_back(valid_checksum ? checksum : static_cast<uint8_t>(checksum ^ 0xFF));
    return frame;
}

bool feed(raven::MspV1Parser& parser,
          const std::vector<uint8_t>& bytes,
          raven::MspFrame& frame)
{
    bool completed = false;
    for (const uint8_t byte : bytes) {
        completed = parser.push(byte, frame) || completed;
    }
    return completed;
}

void encodes_read_request()
{
    std::vector<uint8_t> encoded;
    assert(raven::MspV1Codec::encode_request(1, nullptr, 0, encoded));
    assert((encoded == std::vector<uint8_t>{'$', 'M', '<', 0, 1, 1}));
}

void parses_fragmented_response()
{
    raven::MspV1Parser parser;
    raven::MspFrame frame;
    const auto bytes = response(1, {0, 1, 48});

    assert(!feed(parser,
                 std::vector<uint8_t>(bytes.begin(), bytes.begin() + 3),
                 frame));
    assert(feed(parser,
                std::vector<uint8_t>(bytes.begin() + 3, bytes.end()),
                frame));
    assert(frame.direction == raven::MspDirection::FromFlightController);
    assert(frame.command == 1);
    assert((frame.payload == std::vector<uint8_t>{0, 1, 48}));
}

void resynchronises_after_bad_checksum()
{
    raven::MspV1Parser parser;
    raven::MspFrame frame;
    std::vector<uint8_t> bytes{0x00, 0xFF, 'x'};
    const auto broken = response(2, {'B', 'T', 'F', 'L'}, false);
    const auto valid = response(2, {'B', 'T', 'F', 'L'});
    bytes.insert(bytes.end(), broken.begin(), broken.end());
    bytes.insert(bytes.end(), valid.begin(), valid.end());

    assert(feed(parser, bytes, frame));
    assert(frame.command == 2);
    assert((frame.payload == std::vector<uint8_t>{'B', 'T', 'F', 'L'}));
}

} // namespace

int main()
{
    encodes_read_request();
    parses_fragmented_response();
    resynchronises_after_bad_checksum();
    return 0;
}
