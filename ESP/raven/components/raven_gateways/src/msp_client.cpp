#include "raven_gateways/msp/msp_client.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <utility>

namespace raven {

MspClient::MspClient(ISerialPort& port, size_t max_payload_size)
    : port_(port)
    , parser_(max_payload_size)
{
}

bool MspClient::open()
{
    if (!port_.open()) {
        return false;
    }

    parser_.reset();
    port_.flush_input();
    return true;
}

void MspClient::close()
{
    port_.close();
    parser_.reset();
}

bool MspClient::is_open() const
{
    return port_.is_open();
}

bool MspClient::request(uint8_t command,
                        const void* payload,
                        size_t payload_size,
                        MspFrame& response,
                        uint32_t timeout_ms)
{
    if (!is_open() || timeout_ms == 0) {
        return false;
    }

    if (!MspV1Codec::encode_request(command, payload, payload_size, tx_frame_)) {
        return false;
    }

    // A dedicated MSP UART has no unsolicited producer. Discarding stale bytes
    // here guarantees that a response belongs to the transaction being issued.
    parser_.reset();
    port_.flush_input();

    if (!write_all(tx_frame_, timeout_ms)) {
        return false;
    }

    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    std::array<uint8_t, 64> rx{};

    while (Clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - Clock::now()).count();
        const uint32_t read_timeout = static_cast<uint32_t>(std::max<int64_t>(1, remaining));

        const ssize_t count = port_.read(rx.data(), rx.size(), read_timeout);
        if (count < 0) {
            return false;
        }

        for (ssize_t i = 0; i < count; ++i) {
            MspFrame candidate;
            if (!parser_.push(rx[static_cast<size_t>(i)], candidate)) {
                continue;
            }

            if (candidate.command != command) {
                continue;
            }

            if (candidate.direction == MspDirection::Error) {
                return false;
            }

            if (candidate.direction == MspDirection::FromFlightController) {
                response = std::move(candidate);
                return true;
            }
        }
    }

    return false;
}

bool MspClient::write_all(const std::vector<uint8_t>& frame, uint32_t timeout_ms)
{
    size_t written = 0;
    while (written < frame.size()) {
        const ssize_t count = port_.write(frame.data() + written, frame.size() - written);
        if (count <= 0) {
            return false;
        }
        written += static_cast<size_t>(count);
    }

    return port_.wait_tx_idle(timeout_ms);
}

} // namespace raven
