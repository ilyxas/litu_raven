#include "raven_gateways/betaflight_msp_client.hpp"
#include "raven_platform/ISerialPort.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

class FakeSerialPort final : public raven::ISerialPort {
public:
    bool open() override
    {
        open_ = true;
        return true;
    }

    void close() override
    {
        open_ = false;
        rx_.clear();
    }

    bool is_open() const override
    {
        return open_;
    }

    ssize_t write(const void* data, size_t size) override
    {
        if (!open_ || data == nullptr || size < 6) {
            return -1;
        }

        const auto* bytes = static_cast<const uint8_t*>(data);
        queue_response(bytes[4]);
        return static_cast<ssize_t>(size);
    }

    ssize_t read(void* data, size_t capacity, uint32_t /*timeout_ms*/) override
    {
        if (!open_ || data == nullptr || capacity == 0) {
            return -1;
        }

        const size_t count = std::min<size_t>({capacity, rx_.size(), 2});
        if (count == 0) {
            return 0;
        }

        std::memcpy(data, rx_.data(), count);
        rx_.erase(rx_.begin(), rx_.begin() + static_cast<std::ptrdiff_t>(count));
        return static_cast<ssize_t>(count);
    }

    bool wait_tx_idle(uint32_t /*timeout_ms*/) override
    {
        return open_;
    }

    void flush_input() override
    {
        rx_.clear();
    }

private:
    void queue_response(uint8_t command)
    {
        std::vector<uint8_t> payload;
        if (command == 1) {
            payload = {0, 1, 48};
        } else if (command == 2) {
            payload = {'B', 'T', 'F', 'L'};
        } else if (command == 3) {
            payload = {4, 5, 0};
        }

        uint8_t checksum = static_cast<uint8_t>(payload.size()) ^ command;
        rx_ = {'$', 'M', '>', static_cast<uint8_t>(payload.size()), command};
        for (const uint8_t byte : payload) {
            rx_.push_back(byte);
            checksum ^= byte;
        }
        rx_.push_back(checksum);
    }

    bool open_{false};
    std::vector<uint8_t> rx_;
};

} // namespace

int main()
{
    FakeSerialPort serial;
    raven::MspClient msp(serial);
    raven::BetaflightMspClient betaflight(msp);

    assert(betaflight.open());

    raven::BetaflightIdentity identity;
    assert(betaflight.probe(identity, 100));
    assert(identity.protocol_version == 0);
    assert(identity.api_major == 1);
    assert(identity.api_minor == 48);
    assert(std::strcmp(identity.variant.data(), "BTFL") == 0);
    assert(identity.firmware_version[0] == 4);
    assert(identity.firmware_version[1] == 5);
    assert(identity.firmware_version[2] == 0);
    assert(betaflight.ping(100));

    betaflight.close();
    assert(!betaflight.is_open());
    return 0;
}
