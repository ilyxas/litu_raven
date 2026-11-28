#pragma once
#include "raven_core/component.hpp"

namespace raven::runtime {
// Declares the subsystem-worker role: operations, deadlines and outcomes obey
// the Service contract in the architectural specification. Adds no state,
// scheduling or dispatch behavior; execution still belongs to Component's owner.
class Service : public Component {};
} // namespace raven::runtime
