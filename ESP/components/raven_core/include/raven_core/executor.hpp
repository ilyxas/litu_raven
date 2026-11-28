#pragma once
#include "raven_core/component.hpp"

namespace raven::runtime {
// Used by both FreeRTOS tasks and deterministic tests. Deadlines are phase
// based; missed ticks are coalesced rather than replayed in an unbounded loop.
class Executor {
  public:
    Executor(Component &component, uint32_t period_ms)
        : component_(component), period_(period_ms ? period_ms : 1) {}
    void step(uint64_t now_ms);

  private:
    Component &component_;
    uint32_t period_;
    uint64_t next_tick_{};
};
} // namespace raven::runtime
