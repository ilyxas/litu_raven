#pragma once

// raven_gateways: external protocol adapters.
// Gateways validate external representations and translate them into
// explicit internal contracts without owning behavioural policy.

#include "raven_gateways/decoder.hpp"
#include "raven_gateways/decoder_registry.hpp"
#include "raven_gateways/encoder.hpp"
#include "raven_gateways/encoder_registry.hpp"
#include "raven_gateways/network_header.hpp"
#include "raven_gateways/network_gateway.hpp"
#include "raven_gateways/network_client_gateway.hpp"
#include "raven_gateways/msp/msp_commands.hpp"
#include "raven_gateways/msp/msp_frame.hpp"
#include "raven_gateways/msp/msp_codec.hpp"
#include "raven_gateways/msp/msp_client.hpp"
#include "raven_gateways/betaflight_msp_client.hpp"
