#include "raven_core/dispatcher.hpp"

namespace raven::runtime {
bool Dispatcher::bind(uint8_t endpoint, Inbox& inbox) {
    if (endpoint >= endpoints_.size() || endpoints_[endpoint]) return false;
    endpoints_[endpoint] = &inbox;
    return true;
}

DeliveryReport Dispatcher::deliver(uint8_t destinations, const Message& message) const {
    DeliveryReport report{destinations, 0, 0, Delivery::Accepted};
    if (!destinations) report.reason = Delivery::Invalid;
    for (uint8_t i = 0; i < endpoints_.size(); ++i) {
        const uint8_t bit = static_cast<uint8_t>(1u << i);
        if (!(destinations & bit)) continue;
        const auto result = endpoints_[i] ? endpoints_[i]->post(message) : Delivery::UnknownMessage;
        if (result == Delivery::Accepted) report.accepted |= bit;
        else { report.rejected |= bit; report.reason = result; }
    }
    return report;
}
} // namespace raven::runtime
