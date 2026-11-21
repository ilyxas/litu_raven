#include "raven_gateways/msp/msp_codec.hpp"

namespace raven {

bool MspV1Codec::encode_request(uint8_t command,
                                const void* payload,
                                size_t payload_size,
                                std::vector<uint8_t>& output)
{
    if (payload_size > 255 || (payload_size > 0 && payload == nullptr)) {
        return false;
    }

    output.clear();
    output.reserve(6 + payload_size);
    output.push_back('$');
    output.push_back('M');
    output.push_back('<');

    const auto size_byte = static_cast<uint8_t>(payload_size);
    uint8_t checksum = size_byte ^ command;

    output.push_back(size_byte);
    output.push_back(command);

    const auto* bytes = static_cast<const uint8_t*>(payload);
    for (size_t i = 0; i < payload_size; ++i) {
        output.push_back(bytes[i]);
        checksum ^= bytes[i];
    }

    output.push_back(checksum);
    return true;
}

MspV1Parser::MspV1Parser(size_t max_payload_size)
    : max_payload_size_(max_payload_size)
{
    payload_.reserve(max_payload_size_);
}

void MspV1Parser::reset()
{
    state_ = State::WaitStart;
    direction_ = MspDirection::FromFlightController;
    expected_payload_size_ = 0;
    command_ = 0;
    checksum_ = 0;
    payload_.clear();
}

bool MspV1Parser::push(uint8_t byte, MspFrame& output)
{
    switch (state_) {
        case State::WaitStart:
            if (byte == '$') {
                state_ = State::WaitProtocol;
            }
            break;

        case State::WaitProtocol:
            if (byte == 'M') {
                state_ = State::WaitDirection;
            } else {
                restart_from(byte);
            }
            break;

        case State::WaitDirection:
            if (byte == '<') {
                direction_ = MspDirection::ToFlightController;
            } else if (byte == '>') {
                direction_ = MspDirection::FromFlightController;
            } else if (byte == '!') {
                direction_ = MspDirection::Error;
            } else {
                restart_from(byte);
                break;
            }
            state_ = State::WaitSize;
            break;

        case State::WaitSize:
            if (byte > max_payload_size_) {
                restart_from(byte);
                break;
            }
            expected_payload_size_ = byte;
            checksum_ = byte;
            payload_.clear();
            state_ = State::WaitCommand;
            break;

        case State::WaitCommand:
            command_ = byte;
            checksum_ ^= byte;
            state_ = expected_payload_size_ == 0
                     ? State::WaitChecksum
                     : State::WaitPayload;
            break;

        case State::WaitPayload:
            payload_.push_back(byte);
            checksum_ ^= byte;
            if (payload_.size() == expected_payload_size_) {
                state_ = State::WaitChecksum;
            }
            break;

        case State::WaitChecksum:
            if (byte == checksum_) {
                output.direction = direction_;
                output.command = command_;
                output.payload = payload_;
                reset();
                return true;
            }
            restart_from(byte);
            break;
    }

    return false;
}

void MspV1Parser::restart_from(uint8_t byte)
{
    reset();
    if (byte == '$') {
        state_ = State::WaitProtocol;
    }
}

} // namespace raven
