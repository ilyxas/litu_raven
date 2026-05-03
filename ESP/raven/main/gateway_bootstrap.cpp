#include "gateway_bootstrap.hpp"

#include "raven_gateways/decoder.hpp"
#include "raven_gateways/encoder.hpp"
#include "raven_services/pilot_messages.hpp"
#include "raven_services/telemetry_messages.hpp"
#include "raven_services/telemetry_encoder.hpp"

namespace raven {

// Static decoder instances — must outlive the NetworkGateway task.
static FixedSizeDecoder    s_joystick_decoder(msg_kind::PILOT_INPUT, sizeof(JoystickPayload));
static FixedSizeDecoder    s_start_manual_decoder(msg_kind::NAV_CMD, sizeof(StartManualPayload));
static FixedSizeDecoder    s_halt_manual_decoder(msg_kind::NAV_CMD, sizeof(HaltManualPayload));
static VariableSizeDecoder s_llm_response_decoder(msg_kind::LLM_DATA, 1, 4096);
static FixedSizeDecoder    s_telemetry_active_decoder(msg_kind::TELEMETRY, sizeof(TelemetryActivePayload));

// Static encoder for outbound telemetry — must outlive the client gateway task.
static TelemetryJsonEncoder s_telemetry_encoder;

void configure_navigation_gateway(
    NetworkGateway&        gateway,
    NetworkClientGateway&  client_gateway,
    PilotInputService&     pilot_service,
    NavigationService&     nav_service,
    NavigationActivity&    nav_activity,
    TelemetryService&      telemetry_service
)
{
    // Inbound (NetworkGateway): decoder + route registrations
    gateway.register_decoder(net_msg::JOYSTICK_INPUT,    &s_joystick_decoder);
    gateway.register_decoder(net_msg::START_MANUAL_NAV,  &s_start_manual_decoder);
    gateway.register_decoder(net_msg::HALT_MANUAL_NAV,   &s_halt_manual_decoder);
    gateway.register_decoder(net_msg::LLM_RESPONSE_TEXT, &s_llm_response_decoder);
    gateway.register_decoder(net_msg::TELEMETRY_ACTIVE,  &s_telemetry_active_decoder);

    gateway.register_route(net_msg::JOYSTICK_INPUT,   &pilot_service);
    gateway.register_route(net_msg::START_MANUAL_NAV, &nav_activity);
    gateway.register_route(net_msg::HALT_MANUAL_NAV,  &nav_activity);
    gateway.register_route(net_msg::TELEMETRY_ACTIVE, &telemetry_service);
    // gateway.register_route(net_msg::LLM_RESPONSE_TEXT, &llm_ingress_service);

    // Outbound (NetworkClientGateway): encoder registrations
    client_gateway.register_encoder(msg_kind::TELEMETRY, net_msg::TELEMETRY_DATA, &s_telemetry_encoder);

    (void)nav_service; // reserved for future internal-only navigation commands
}

} // namespace raven
