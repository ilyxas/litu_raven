#pragma once


// raven_platform: hardware and ESP-IDF platform abstractions.
// Provides thin wrappers and initialisation helpers that isolate the
// rest of the runtime from direct ESP-IDF driver dependencies.

#include "raven_gateways/decoder.hpp"
#include "raven_gateways/decoder_registry.hpp"
#include "raven_gateways/encoder.hpp"
#include "raven_gateways/encoder_registry.hpp"
#include "raven_gateways/network_header.hpp"
#include "raven_gateways/raven_gateways.hpp"
