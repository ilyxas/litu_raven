#pragma once

#include "raven_activities/base_activity.hpp"

// raven_activities: high-level orchestration / FSM behaviour layer.
// Concrete product activities are introduced only when a real orchestration
// responsibility exists. The architectural role remains available even when
// a product branch currently has no concrete Activity.
