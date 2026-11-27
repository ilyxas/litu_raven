#include "raven_services/peer_contracts.hpp"

namespace raven::services {
using comms::Failure;
const char *name(PeerStatus s) {
    switch (s) {
    case PeerStatus::Unknown:
        return "unknown";
    case PeerStatus::Handshaking:
        return "handshaking";
    case PeerStatus::NotReady:
        return "not-ready";
    case PeerStatus::Ready:
        return "ready";
    case PeerStatus::Disconnected:
        return "disconnected";
    }
    return "invalid";
}
const char *name(Outcome o) {
    switch (o) {
    case Outcome::Pending:
        return "pending";
    case Outcome::Succeeded:
        return "success";
    case Outcome::LocalRejected:
        return "local-rejected";
    case Outcome::TransportLost:
        return "transport-lost";
    case Outcome::SendDeadline:
        return "send-deadline";
    case Outcome::RemoteNack:
        return "remote-nack";
    case Outcome::ResponseTimeout:
        return "response-timeout";
    case Outcome::Rejected:
        return "rejected";
    }
    return "invalid";
}
const char *name(Failure f) {
    switch (f) {
    case Failure::None:
        return "none";
    case Failure::TransportLost:
        return "transport-lost";
    case Failure::Protocol:
        return "protocol";
    case Failure::LocalFull:
        return "local-full";
    case Failure::SendDeadline:
        return "send-deadline";
    case Failure::WriteError:
        return "write-error";
    case Failure::ResponseTimeout:
        return "response-timeout";
    case Failure::RemoteNack:
        return "remote-nack";
    case Failure::IdentityConflict:
        return "identity-conflict";
    }
    return "invalid";
}
} // namespace raven::services
