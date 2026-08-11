#include "raven_state/flight_controller_state.hpp"

namespace raven {

FlightControllerSnapshot FlightControllerState::snapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return snapshot_;
}

void FlightControllerState::mark_connected(const FlightControllerIdentity& identity,
                                           uint32_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.link_status = FlightControllerLinkStatus::Connected;
    snapshot_.identity = identity;
    snapshot_.last_success_ms = now_ms;
    snapshot_.consecutive_failures = 0;
    ++snapshot_.successful_transactions;
    ++snapshot_.generation;
}

void FlightControllerState::record_success(uint32_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.last_success_ms = now_ms;
    snapshot_.consecutive_failures = 0;
    ++snapshot_.successful_transactions;
    ++snapshot_.generation;
}

void FlightControllerState::record_failure(uint32_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.last_failure_ms = now_ms;
    ++snapshot_.failed_transactions;
    ++snapshot_.consecutive_failures;
    ++snapshot_.generation;
}

void FlightControllerState::mark_disconnected()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (snapshot_.link_status == FlightControllerLinkStatus::Disconnected) {
        return;
    }

    snapshot_.link_status = FlightControllerLinkStatus::Disconnected;
    ++snapshot_.generation;
}

} // namespace raven
