#pragma once

#include "raven_services/base_service.hpp"
#include "raven_services/navigation_messages.hpp"
#include "raven_services/navigation_service.hpp"
#include "raven_services/pilot_input_service.hpp"
#include "raven_services/telemetry_messages.hpp"
#include "raven_services/telemetry_service.hpp"

// raven_services: asynchronous subsystem workers.
// Services encapsulate interaction with a single hardware subsystem or
// software stack, run in their own FreeRTOS task where appropriate,
// and write results into shared state via controllers.
