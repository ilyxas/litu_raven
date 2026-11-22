#pragma once
#include "raven_core/inbox.hpp"

namespace raven::runtime {
// Behavioral owner: subscribe in construction; only its Executor calls the
// handlers. Callbacks/producers post copies to inbox instead of invoking them.
class Component {
public:
    virtual ~Component() = default;
    Inbox inbox;
    virtual void on_message(const Message&, uint64_t now_ms) = 0;
    virtual void on_tick(uint64_t now_ms) = 0;
};
} // namespace raven::runtime
