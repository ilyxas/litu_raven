#pragma once

#include "esp_event.h"

// NAVIGATION_EVENTS — event base for NavigationService completion signals.
//
// These events are published by NavigationService and consumed by
// NavigationActivity (and any other interested subscriber).

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(NAVIGATION_EVENTS);

// Event IDs within the NAVIGATION_EVENTS base.
enum NavigationEventId {
    NAV_EVENT_MOVE_FORWARD_DONE = 0,  // NavigationService completed a move-forward command
};

#ifdef __cplusplus
}
#endif
