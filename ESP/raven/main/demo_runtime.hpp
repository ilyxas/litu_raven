#pragma once
#include "raven_platform/sim_arm_driver.hpp"
#include "raven_platform/owner_task.hpp"
#include "raven_services/arm_service.hpp"
#include "raven_services/sim_sensor_service.hpp"
#include "raven_activities/dual_arm_activity.hpp"
#include "raven_gateways/command_server.hpp"
#include "raven_gateways/telemetry_publisher.hpp"
#include "sdkconfig.h"
#ifdef CONFIG_RAVEN_BLE_ENABLED
#include "raven_gateways/ble_control_gateway.hpp"
#endif

namespace raven::demo {
// Static composition root: declaration order encodes reference lifetimes.
// Only bootstrap binds routes and starts owners/transports; no behavior lives here.
struct Runtime {
    SimArmDriver left_driver, right_driver;
    ArmService left{left_driver}, right{right_driver};
    Dispatcher dispatcher;
    DualArmActivity activity{dispatcher, left.state, right.state};
    Protocol protocol{dispatcher, left.state, right.state, activity.state};
    SimSensorService sensors;
    OwnerTask left_task{left, "left_arm", 5}, right_task{right, "right_arm", 5};
    OwnerTask activity_task{activity, "activity", 4}, sensor_task{sensors, "sensors", 3, 50};
    CommandServer commands{protocol, CONFIG_RAVEN_RND_PORT};
    TelemetryPublisher telemetry{left.state, right.state, sensors.state,
                                 CONFIG_RAVEN_RND_TELEMETRY_HOST, CONFIG_RAVEN_RND_TELEMETRY_PORT};
#ifdef CONFIG_RAVEN_BLE_ENABLED
    SerialControlProtocol serial_protocol{dispatcher, left.state, right.state, activity.state};
    BleControlGateway ble{serial_protocol, sensors.state};
#endif
};
void start_demo_runtime();
} // namespace raven::demo
