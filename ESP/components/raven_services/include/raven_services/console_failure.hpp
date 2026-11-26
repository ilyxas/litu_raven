#pragma once
#include <cassert>
#include <cstdint>

namespace raven::services {
// Composition-supplied failure boundary for a mandatory console reply. This
// requests closure of that console generation, never execution of another
// Service. The provider must be thread-safe and ignore obsolete generations.
struct ConsoleFailure {
    void (*close)(void *, uint32_t);
    void *context{};
    void operator()(uint32_t generation) const {
        assert(close);
        close(context, generation);
    }
};
} // namespace raven::services
