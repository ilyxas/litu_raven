#include "raven_icd/device_protocol.hpp"
#include <cstring>

namespace raven::icd {
namespace {
void put(uint8_t *&p, uint64_t value, unsigned n) {
    while (n) {
        --n;
        *p++ = uint8_t(value >> (n * 8));
    }
}
uint64_t get(const uint8_t *&p, unsigned n) {
    uint64_t v = 0;
    while (n--)
        v = (v << 8) | *p++;
    return v;
}
bool valid_readiness(Readiness v) {
    return v <= Readiness::Rejected;
}
} // namespace
bool known(Type type) {
    return type >= Type::HelloRequest && type <= Type::Diagnostic;
}
bool request(Type type) {
    return type == Type::HelloRequest || type == Type::KeepAliveRequest;
}
size_t encode(const Packet &packet, uint8_t *out, size_t capacity) {
    if (!out || !known(packet.type) || (packet.requires_ack && !request(packet.type)))
        return 0;
    size_t body_size = 0;
    switch (packet.type) {
    case Type::HelloRequest:
    case Type::HelloResponse:
        body_size = 48;
        break;
    case Type::KeepAliveRequest:
    case Type::KeepAliveResponse:
        body_size = 20;
        break;
    case Type::Receipt:
        body_size = 4;
        break;
    case Type::Diagnostic:
        if (packet.body.get<Diagnostic>().size > sizeof(Diagnostic::text))
            return 0;
        body_size = 16 + packet.body.get<Diagnostic>().size;
        break;
    }
    const auto total = header_size + body_size;
    if (capacity < total || (packet.type != Type::Diagnostic && packet.exchange == 0))
        return 0;
    auto *p = out;
    put(p, version, 1);
    put(p, packet.requires_ack ? 1 : 0, 1);
    put(p, uint16_t(packet.type), 2);
    put(p, packet.exchange, 4);
    switch (packet.type) {
    case Type::HelloRequest:
    case Type::HelloResponse: {
        const auto h = packet.body.get<Hello>();
        const auto &d = h.device;
        put(p, d.device, 8);
        put(p, d.boot, 8);
        put(p, d.capabilities, 2);
        put(p, d.protocol, 1);
        put(p, uint8_t(d.role), 1);
        put(p, uint8_t(d.readiness), 1);
        put(p, d.flags, 1);
        put(p, uint16_t(d.reason), 2);
        std::memcpy(p, d.name, sizeof(d.name));
        p += sizeof(d.name);
        put(p, h.uptime_ms, 8);
        break;
    }
    case Type::KeepAliveRequest:
    case Type::KeepAliveResponse: {
        const auto h = packet.body.get<Heartbeat>();
        put(p, h.origin_ms, 8);
        put(p, h.uptime_ms, 8);
        put(p, uint16_t(h.reason), 2);
        put(p, uint8_t(h.readiness), 1);
        put(p, 0, 1);
        break;
    }
    case Type::Receipt:
        put(p, uint16_t(packet.body.get<Receipt>().request_type), 2);
        put(p, uint8_t(packet.body.get<Receipt>().admission), 1);
        put(p, 0, 1);
        break;
    case Type::Diagnostic: {
        const auto d = packet.body.get<Diagnostic>();
        put(p, d.source_ms, 8);
        put(p, d.sequence, 4);
        put(p, d.code, 2);
        put(p, d.severity, 1);
        put(p, d.size, 1);
        std::memcpy(p, d.text, d.size);
        break;
    }
    }
    return total;
}
Decode decode(const uint8_t *bytes, size_t size, Packet &packet) {
    packet = {};
    if (!bytes || size < header_size || size > max_packet)
        return Decode::Invalid;
    const auto *p = bytes;
    const auto wire_version = get(p, 1), flags = get(p, 1);
    packet.type = Type(get(p, 2));
    packet.exchange = uint32_t(get(p, 4));
    packet.requires_ack = (flags & 1) != 0;
    if (wire_version != version)
        return Decode::UnsupportedVersion;
    if (flags & ~1u)
        return Decode::Invalid;
    if (!known(packet.type))
        return Decode::UnknownType;
    if ((packet.requires_ack && !request(packet.type)) ||
        (packet.type != Type::Diagnostic && !packet.exchange))
        return Decode::Invalid;
    const auto body_size = size - header_size;
    switch (packet.type) {
    case Type::HelloRequest:
    case Type::HelloResponse: {
        if (body_size != 48)
            return Decode::Invalid;
        Hello h{};
        auto &d = h.device;
        d.device = get(p, 8);
        d.boot = get(p, 8);
        d.capabilities = uint16_t(get(p, 2));
        d.protocol = uint8_t(get(p, 1));
        d.role = Role(get(p, 1));
        d.readiness = Readiness(get(p, 1));
        d.flags = uint8_t(get(p, 1));
        d.reason = Reason(get(p, 2));
        std::memcpy(d.name, p, sizeof(d.name));
        p += sizeof(d.name);
        h.uptime_ms = get(p, 8);
        if (!d.device || !d.boot || !valid_readiness(d.readiness) ||
            (d.role != Role::MissionComputer && d.role != Role::Node) || d.flags)
            return Decode::Invalid;
        packet.body.set(h);
        break;
    }
    case Type::KeepAliveRequest:
    case Type::KeepAliveResponse: {
        if (body_size != 20)
            return Decode::Invalid;
        Heartbeat h{};
        h.origin_ms = get(p, 8);
        h.uptime_ms = get(p, 8);
        h.reason = Reason(get(p, 2));
        h.readiness = Readiness(get(p, 1));
        h.reserved = uint8_t(get(p, 1));
        if (!valid_readiness(h.readiness) || h.reserved)
            return Decode::Invalid;
        packet.body.set(h);
        break;
    }
    case Type::Receipt: {
        if (body_size != 4)
            return Decode::Invalid;
        Receipt r{};
        r.request_type = Type(get(p, 2));
        r.admission = Admission(get(p, 1));
        r.reserved = uint8_t(get(p, 1));
        if (!request(r.request_type) || r.admission > Admission::Unknown || r.reserved)
            return Decode::Invalid;
        packet.body.set(r);
        break;
    }
    case Type::Diagnostic: {
        if (body_size < 16)
            return Decode::Invalid;
        Diagnostic d{};
        d.source_ms = get(p, 8);
        d.sequence = uint32_t(get(p, 4));
        d.code = uint16_t(get(p, 2));
        d.severity = uint8_t(get(p, 1));
        d.size = uint8_t(get(p, 1));
        if (d.size > sizeof(d.text) || body_size != 16u + d.size || d.severity > 3)
            return Decode::Invalid;
        std::memcpy(d.text, p, d.size);
        packet.body.set(d);
        break;
    }
    }
    return Decode::Ok;
}
} // namespace raven::icd
