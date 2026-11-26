#pragma once
#include "raven_comms/session.hpp"

namespace raven::services {
// Shared peer observations and command/result payloads. Consumers depend on
// these contracts, not on the service that executes the peer exchange.
enum class PeerStatus : uint8_t { Unknown, Handshaking, NotReady, Ready, Disconnected };
struct PeerRecord {
    icd::DeviceDescriptor descriptor{};
    comms::SessionKey route{};
    uint64_t last_seen{}, changed_at{};
    PeerStatus status{};
    comms::Failure failure{};
};
enum class Outcome : uint8_t {
    Pending,
    Succeeded,
    LocalRejected,
    TransportLost,
    SendDeadline,
    RemoteNack,
    ResponseTimeout,
    Rejected
};
constexpr uint16_t probe_request_id = 0x300, probe_result_id = 0x301;
struct ProbeRequest {
    icd::DeviceId device{};
    uint32_t console_generation{}, request{};
};
struct ProbeResult {
    icd::DeviceId device{};
    uint64_t elapsed_ms{};
    uint32_t console_generation{}, request{};
    Outcome outcome{};
};
const char *name(PeerStatus status);
const char *name(Outcome result);
const char *name(comms::Failure failure);
} // namespace raven::services
