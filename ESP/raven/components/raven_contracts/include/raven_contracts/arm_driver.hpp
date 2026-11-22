#pragma once
#include "raven_contracts/arm_commands.hpp"

namespace raven::demo {
struct DriverStatus { float position{}; bool ready{}, reached{}, fault{}; };
// Device API owned by ArmService. Calls make bounded progress; completion may
// arrive by callback or read(). Driver must outlive the service and stop callbacks
// before destroying it. Callback is task-context, not an ISR entry.
class IArmDriver {
public:
    using Callback = void (*)(void*, const Completion&);
    virtual ~IArmDriver() = default;
    virtual void begin(float target, uint64_t generation, uint64_t now) = 0;
    virtual void stop() = 0;
    virtual void advance(uint64_t now) = 0;
    virtual DriverStatus read() const = 0;
    virtual void configure(DriverMode mode) = 0;
    virtual void set_callback(Callback callback, void* context) = 0;
};
} // namespace raven::demo
