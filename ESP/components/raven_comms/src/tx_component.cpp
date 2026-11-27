#include "raven_comms/tx_component.hpp"
#include <cassert>
namespace raven::comms {
TxComponent::TxComponent(Session &session, PacketWriter &writer, Clock clock)
    : session_(session), writer_(writer), clock_(clock) {
    for (uint16_t id = uint16_t(icd::Type::HelloRequest); id <= uint16_t(icd::Type::Diagnostic); ++id) {
        const bool subscribed =
            inbox.subscribe(outgoing(icd::Type(id)), sizeof(Outbound), runtime::Policy::Ordered);
        assert(subscribed);
        (void)subscribed;
    }
}
void TxComponent::on_message(const runtime::Message &message, uint64_t) {
    Outbound value{};
    if (!message.read(value))
        return;
    uint64_t deadline{};
    if (!session_.begin(value.ticket, value.route, clock_.now(), deadline))
        return;
    icd::Packet packet{};
    packet.type = icd::Type(message.id - outgoing_base);
    packet.exchange = message.request;
    packet.requires_ack = packet.type == icd::Type::HelloRequest;
    packet.body = value.body;
    const auto size = icd::encode(packet, bytes_.data(), bytes_.size());
    const auto failure =
        size ? writer_.write_packet(value.route, bytes_.data(), size, deadline) : Failure::Protocol;
    session_.finish(value.ticket, value.route, clock_.now(), failure);
    if (packet.type == icd::Type::Diagnostic)
        session_.finish_diagnostic(value.route);
    if (failure != Failure::None)
        session_.close(value.route, failure);
}
} // namespace raven::comms
