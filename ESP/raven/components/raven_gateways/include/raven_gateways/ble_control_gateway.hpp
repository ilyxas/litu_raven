#pragma once
#include "raven_gateways/serial_control_protocol.hpp"
#include "raven_platform/ble_serial_server.hpp"
#include "raven_state/sensor_state.hpp"
#include "freertos/task.h"

namespace raven::demo {
// Connects a byte transport to the demo text adapter. Only this task parses;
// the NimBLE callback copies input. Periodic UI readouts use published snapshots.
class BleControlGateway {
  public:
    BleControlGateway(SerialControlProtocol &protocol, const SensorState &sensors)
        : protocol_(protocol), sensors_(sensors) {}
    void start();
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }

  private:
    static bool emit(void *ctx, const char *bytes, size_t size);
    void run();
    SerialControlProtocol &protocol_;
    const SensorState &sensors_;
    BleSerialServer server_;
    uint32_t session_{};
    TaskHandle_t handle_{};
};
} // namespace raven::demo
