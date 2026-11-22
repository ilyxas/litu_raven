#pragma once
#include "raven_core/dispatcher.hpp"
#include "raven_state/operation_state.hpp"
#include "raven_contracts/arm_commands.hpp"

namespace raven::demo {
using namespace raven::runtime;
namespace wire {
constexpr uint16_t move = 0x1101, control = 0x1102, target = 0x1103, cut = 0x1104;
constexpr uint16_t status = 0x1105, configure = 0x1106, pair = 0x1107;
constexpr uint16_t ack = 0x7f01, nack = 0x7f02, snapshot = 0x7f03;
constexpr size_t max_payload = 128;
uint32_t u32(const uint8_t* bytes);
void put32(uint8_t* bytes, uint32_t value);
}
struct Response {
    uint16_t id{};
    uint32_t size{};
    std::array<uint8_t, wire::max_payload> payload{};
};

// Binary command adapter. Validates wire structure and returns admission receipts
// immediately; Status reads published state. One serialized caller per instance.
class Protocol {
public:
    Protocol(Dispatcher& dispatcher, const OperationState& left, const OperationState& right,
             const OperationState& activity)
        : dispatcher_(dispatcher), states_{&left, &right, &activity} {}
    Response handle(uint16_t id, const uint8_t* data, size_t size);
private:
    Response receipt(uint16_t id, uint32_t request, const DeliveryReport& report) const;
    Dispatcher& dispatcher_;
    std::array<const OperationState*, 3> states_;
    uint32_t legacy_next_{0x40000000u}, legacy_left_{}, legacy_right_{};
};
} // namespace raven::demo
