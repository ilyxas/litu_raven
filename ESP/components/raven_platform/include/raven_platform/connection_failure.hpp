#pragma once
#include <atomic>
#include <cstdint>

namespace raven {
// Record a failure only for the current connection. A delayed producer cannot
// overwrite a failure for a newer generation, including across a competing CAS.
inline void fail_connection_generation(const std::atomic<uint32_t> &current, std::atomic<uint32_t> &failed,
                                       uint32_t generation) {
    auto observed = failed.load();
    while (generation == current.load() && !failed.compare_exchange_weak(observed, generation)) {
    }
}
} // namespace raven
