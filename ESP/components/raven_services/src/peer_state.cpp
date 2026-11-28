#include "raven_services/peer_state.hpp"

namespace raven::services {
bool PeerState::peer(size_t index, PeerRecord &value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= capacity_ || !records_[index].descriptor.device)
        return false;
    value = records_[index];
    if (value.status != PeerStatus::Disconnected &&
        (value.route.slot >= session_count_ || !sessions_[value.route.slot].active(value.route)))
        value.status = PeerStatus::Disconnected;
    return true;
}
uint32_t PeerState::stale_messages() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stale_;
}
} // namespace raven::services
