#pragma once
#include "raven_core/component.hpp"

namespace raven::runtime {
// Declares the orchestration role: scenario/mode decisions obey the Activity
// contract in the architectural specification. Adds no execution machinery.
// Adapters and other components need not declare a behavioral role.
class Activity : public Component {};
} // namespace raven::runtime
