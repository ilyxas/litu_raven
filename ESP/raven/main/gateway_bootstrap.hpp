#pragma once

// gateway_bootstrap.hpp — wiring helper for the navigation/pilot gateway.
//
// Encapsulates static decoder/encoder instances and the registration of
// decoders, encoders, and routes. app_main calls configure_navigation_gateway()
// once after constructing all long-lived objects.

#include "raven_gateways/network_gateway.hpp"
#include "raven_gateways/network_client_gateway.hpp"
#include "raven_services/navigation_service.hpp"
#include "raven_services/pilot_input_service.hpp"
#include "raven_services/telemetry_service.hpp"
#include "raven_activities/navigation_activity.hpp"

namespace raven {

// Register all decoders, encoders, and routes.
// All registered codec objects have static storage duration and outlive the
// gateway tasks.
void configure_navigation_gateway(
    NetworkGateway&        gateway,
    NetworkClientGateway&  client_gateway,
    PilotInputService&     pilot_service,
    NavigationService&     nav_service,
    NavigationActivity&    nav_activity,
    TelemetryService&      telemetry_service
);

} // namespace raven
