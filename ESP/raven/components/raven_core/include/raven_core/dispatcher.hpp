#pragma once
#include "raven_core/inbox.hpp"

namespace raven::runtime {
struct DeliveryReport {
    uint8_t requested{}, accepted{}, rejected{};
    Delivery reason{Delivery::Accepted};
    bool complete() const { return requested != 0 && accepted == requested; }
};

// Routes to explicitly selected endpoints. Bind once at bootstrap; delivery is
// synchronous admission, with per-destination results and no fanout rollback.
class Dispatcher {
public:
    bool bind(uint8_t endpoint, Inbox& inbox); // Immutable once tasks start.
    DeliveryReport deliver(uint8_t destinations, const Message& message) const;
private:
    std::array<Inbox*, 8> endpoints_{};
};
} // namespace raven::runtime
