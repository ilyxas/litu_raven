#include "raven_gateways/betaflight_msp_client.hpp"

#include "raven_gateways/msp/msp_commands.hpp"

#include <algorithm>

namespace raven {

BetaflightMspClient::BetaflightMspClient(MspClient& msp)
    : msp_(msp)
{
}

bool BetaflightMspClient::open()
{
    return msp_.open();
}

void BetaflightMspClient::close()
{
    msp_.close();
}

bool BetaflightMspClient::is_open() const
{
    return msp_.is_open();
}

bool BetaflightMspClient::probe(BetaflightIdentity& identity, uint32_t timeout_ms)
{
    MspFrame response;
    if (!request(MspCommand::ApiVersion, response, timeout_ms) || response.payload.size() < 3) {
        return false;
    }
    identity.protocol_version = response.payload[0];
    identity.api_major = response.payload[1];
    identity.api_minor = response.payload[2];

    if (!request(MspCommand::FlightControllerVariant, response, timeout_ms) ||
        response.payload.size() < 4) {
        return false;
    }
    std::copy_n(response.payload.begin(), 4, identity.variant.begin());
    identity.variant[4] = '\0';

    if (!request(MspCommand::FlightControllerVersion, response, timeout_ms) ||
        response.payload.size() < identity.firmware_version.size()) {
        return false;
    }
    std::copy_n(response.payload.begin(),
                identity.firmware_version.size(),
                identity.firmware_version.begin());
    return true;
}

bool BetaflightMspClient::ping(uint32_t timeout_ms)
{
    MspFrame response;
    return request(MspCommand::ApiVersion, response, timeout_ms) &&
           response.payload.size() >= 3;
}

bool BetaflightMspClient::request(MspCommand command,
                                  MspFrame& response,
                                  uint32_t timeout_ms)
{
    return msp_.request(static_cast<uint8_t>(command),
                        nullptr,
                        0,
                        response,
                        timeout_ms);
}

} // namespace raven
