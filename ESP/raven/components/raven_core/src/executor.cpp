#include "raven_core/executor.hpp"

namespace raven::runtime {
void Executor::step(uint64_t now_ms) {
    Message message;
    // At most the fixed subscription count: continuous critical ingress cannot
    // indefinitely starve timeout/status servicing.
    for (size_t i = 0; i < 8 && component_.inbox.take_critical(message); ++i)
        component_.on_message(message, now_ms);
    if (now_ms >= next_tick_) {
        component_.on_tick(now_ms);
        next_tick_ = now_ms - now_ms % period_ + period_;
    }
    if (component_.inbox.take_critical(message)) component_.on_message(message, now_ms);
    else if (component_.inbox.take_normal(message)) component_.on_message(message, now_ms);
}
} // namespace raven::runtime
