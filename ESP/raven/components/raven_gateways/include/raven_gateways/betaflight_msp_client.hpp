#pragma once

#include "raven_gateways/msp/msp_client.hpp"
#include "raven_gateways/msp/msp_commands.hpp"

#include <array>
#include <cstdint>

namespace raven {

struct BetaflightIdentity {
    uint8_t protocol_version{0};
    uint8_t api_major{0};
    uint8_t api_minor{0};
    std::array<char, 5> variant{};
    std::array<uint8_t, 3> firmware_version{};
};

class IBetaflightClient {
public:
    virtual ~IBetaflightClient() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
    virtual bool probe(BetaflightIdentity& identity, uint32_t timeout_ms) = 0;
    virtual bool ping(uint32_t timeout_ms) = 0;
};

// Read-only Betaflight client. No MSP_SET_* operation is exposed.
class BetaflightMspClient final : public IBetaflightClient {
public:
    explicit BetaflightMspClient(MspClient& msp);

    bool open() override;
    void close() override;
    bool is_open() const override;
    bool probe(BetaflightIdentity& identity, uint32_t timeout_ms) override;
    bool ping(uint32_t timeout_ms) override;

private:
    bool request(MspCommand command, MspFrame& response, uint32_t timeout_ms);

    MspClient& msp_;
};

} // namespace raven
