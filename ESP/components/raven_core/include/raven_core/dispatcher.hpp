#pragma once
#include "raven_core/inbox.hpp"

namespace raven::runtime {
using DestinationMask = uint16_t;
constexpr DestinationMask destination(uint8_t endpoint) {
    return endpoint < 16 ? DestinationMask(1u << endpoint) : 0;
}
struct DeliveryReport {
    DestinationMask requested{}, accepted{}, rejected{};
    Delivery reason{Delivery::Accepted};
    bool complete() const { return requested != 0 && accepted == requested; }
};

// Routes to explicitly selected endpoints. Bind once at bootstrap; delivery is
// synchronous admission, with per-destination results and no fanout rollback.
class Dispatcher {
  public:
    bool bind(uint8_t endpoint, Inbox &inbox); // Immutable once tasks start.
    DeliveryReport deliver(DestinationMask destinations, const Message &message) const;

  private:
    std::array<Inbox *, 16> endpoints_{};
};
} // namespace raven::runtime
