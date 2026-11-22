#include "raven_gateways/command_protocol.hpp"
#include <cmath>

namespace raven::demo {
namespace wire {
uint32_t u32(const uint8_t* b) {
    return uint32_t(b[0]) | uint32_t(b[1]) << 8 | uint32_t(b[2]) << 16 | uint32_t(b[3]) << 24;
}
void put32(uint8_t* b, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) b[i] = static_cast<uint8_t>(value >> (8 * i));
}
}
namespace {
float get_float(const uint8_t* b) { const auto bits = wire::u32(b); float value; std::memcpy(&value, &bits, 4); return value; }
void put_float(uint8_t* b, float value) { uint32_t bits; std::memcpy(&bits, &value, 4); wire::put32(b, bits); }
DeliveryReport combine(const DeliveryReport& a, const DeliveryReport& b) {
    return {static_cast<uint8_t>(a.requested | b.requested), static_cast<uint8_t>(a.accepted | b.accepted),
            static_cast<uint8_t>(a.rejected | b.rejected), a.complete() ? b.reason : a.reason};
}
}
Response Protocol::receipt(uint16_t id, uint32_t request, const DeliveryReport& report) const {
    Response response;
    response.id = report.complete() ? wire::ack : wire::nack;
    response.size = 9;
    wire::put32(response.payload.data(), request);
    response.payload[4] = static_cast<uint8_t>(id);
    response.payload[5] = static_cast<uint8_t>(id >> 8);
    response.payload[6] = static_cast<uint8_t>(report.reason);
    response.payload[7] = report.accepted;
    response.payload[8] = report.rejected;
    return response;
}
Response Protocol::handle(uint16_t id, const uint8_t* data, size_t size) {
    const uint32_t request = size >= 4 ? wire::u32(data) : 0;
    const auto invalid = [&] { return receipt(id, request, {0, 0, 0, Delivery::Invalid}); };
    // Legacy iOS workbench: no request ID in payload. Preserve its existing
    // two-connection transport and translate at the boundary only.
    if (id == 0x1002 && size == 4) {
        legacy_left_ = legacy_next_++;
        legacy_right_ = legacy_next_++;
        return receipt(id, 0, combine(dispatcher_.deliver(1, Message::make(Control, legacy_left_, Goal{})),
                                     dispatcher_.deliver(2, Message::make(Control, legacy_right_, Goal{}))));
    }
    if (id == 0x1003 && size == 4)
        return receipt(id, 0, dispatcher_.deliver(7, Message::make(Cut, 0, Empty{})));
    if (id == 0x1001 && size == 12) {
        const float left = get_float(data), right = get_float(data + 4);
        if (!std::isfinite(left) || !std::isfinite(right) || !legacy_left_ || !legacy_right_) return invalid();
        return receipt(id, 0, combine(
            dispatcher_.deliver(1, Message::make(Target, 0, TargetValue{legacy_left_, left * 90})),
            dispatcher_.deliver(2, Message::make(Target, 0, TargetValue{legacy_right_, right * 90}))));
    }
    if (!request || request >= 0x40000000u) return invalid();
    if (id == wire::status && size == 9 && data[4] < states_.size()) {
        const auto value = states_[data[4]]->find(wire::u32(data + 5));
        Response response;
        response.id = wire::snapshot;
        response.size = 27;
        auto* out = response.payload.data();
        wire::put32(out, request);
        out[4] = data[4];
        wire::put32(out + 5, value.operation);
        out[9] = static_cast<uint8_t>(value.status);
        put_float(out + 10, value.position);
        put_float(out + 14, value.target);
        out[18] = value.continuous;
        wire::put32(out + 19, static_cast<uint32_t>(value.changed_ms));
        wire::put32(out + 23, static_cast<uint32_t>(value.changed_ms >> 32));
        return response;
    }
    if ((id == wire::move || id == wire::control) && size == 9 && data[4] < 2) {
        const float value = get_float(data + 5);
        if (!std::isfinite(value)) return invalid();
        return receipt(id, request, dispatcher_.deliver(1u << data[4],
            Message::make(id == wire::move ? Move : Control, request, Goal{value})));
    }
    if (id == wire::target && size == 13 && data[4] < 2) {
        const float value = get_float(data + 9);
        if (!std::isfinite(value)) return invalid();
        return receipt(id, request, dispatcher_.deliver(1u << data[4],
            Message::make(Target, request, TargetValue{wire::u32(data + 5), value})));
    }
    if (id == wire::cut && size == 5 && data[4] && data[4] <= 7) {
        // A scenario cut explicitly targets all three mandatory recipients.
        const uint8_t mask = data[4] & 4 ? 7 : data[4];
        return receipt(id, request, dispatcher_.deliver(mask, Message::make(Cut, request, Empty{})));
    }
    if (id == wire::configure && size == 6 && data[4] < 2 && data[5] <= uint8_t(DriverMode::Error))
        return receipt(id, request, dispatcher_.deliver(1u << data[4],
            Message::make(Configure, request, Configuration{static_cast<DriverMode>(data[5])})));
    if (id == wire::pair && size == 12) {
        const PairGoal goal{get_float(data + 4), get_float(data + 8)};
        if (!std::isfinite(goal.left) || !std::isfinite(goal.right)) return invalid();
        return receipt(id, request, dispatcher_.deliver(4, Message::make(Pair, request, goal)));
    }
    return invalid();
}
} // namespace raven::demo
