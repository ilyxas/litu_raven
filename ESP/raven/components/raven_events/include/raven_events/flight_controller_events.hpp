#pragma once

#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(FLIGHT_CONTROLLER_EVENTS);

enum FlightControllerEventId {
    FC_EVENT_LINK_UP = 0,
    FC_EVENT_LINK_DOWN,
};

#ifdef __cplusplus
}
#endif
