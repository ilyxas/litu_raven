#pragma once

#include "raven_activities/base_activity.hpp"
#include "raven_activities/navigation_activity.hpp"

// raven_activities: high-level orchestration / FSM behaviour layer.
// Activities represent the mode the vehicle is operating in and own
// the decision flow for transitioning between those modes.
// Each activity owns a FreeRTOS task and an inbound command queue.
