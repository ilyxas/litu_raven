#pragma once

#include "raven_state/navigation_state.hpp"
#include "raven_state/telemetry_state.hpp"

// raven_state: thread-safe shared state controllers.
// Controllers are the single source of truth for system-wide data.
// They expose read/write APIs and protect internal data with mutexes or atomics.
// Controllers do not own FreeRTOS tasks and contain no business logic.
