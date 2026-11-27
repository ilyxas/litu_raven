#pragma once
#include "raven_services/peer_contracts.hpp"
#include <cassert>
#include <mutex>

namespace raven::services {
// Canonical peer knowledge, independent of PeerService behavior. Composition
// owns the backing records and sessions; observers receive a const State API.
// Snapshot validity includes the session generation, even before the owner has
// processed close. This store performs no I/O, dispatch, or autonomous work.
class PeerState {
  public:
    PeerState(const icd::DeviceDescriptor &local, PeerRecord *records, size_t capacity,
              const comms::Session *sessions, size_t session_count)
        : local_(local), records_(records), capacity_(capacity), sessions_(sessions),
          session_count_(session_count) {
        assert((records || !capacity) && (sessions || !session_count));
    }
    bool peer(size_t index, PeerRecord &value) const;
    size_t capacity() const { return capacity_; }
    icd::DeviceDescriptor identity() const { return local_; }
    uint32_t stale_messages() const;

    // The designated writer holds this transaction across a handler's updates.
    // Record references are valid only while the transaction is held. Acquire
    // it before Session::Guard: PeerState -> Session -> Inbox is the lock order.
    class Write {
      public:
        explicit Write(PeerState &state) : lock_(state.mutex_), state_(state) {}
        PeerRecord &record(size_t index) {
            assert(index < state_.capacity_);
            return state_.records_[index];
        }
        size_t capacity() const { return state_.capacity_; }
        void note_stale() { ++state_.stale_; }

      private:
        std::unique_lock<std::mutex> lock_;
        PeerState &state_;
    };

  private:
    const icd::DeviceDescriptor local_;
    PeerRecord *records_;
    size_t capacity_;
    const comms::Session *sessions_;
    size_t session_count_;
    mutable std::mutex mutex_;
    uint32_t stale_{};
};
} // namespace raven::services
