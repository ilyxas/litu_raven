#pragma once
#include "raven_platform/ble_serial_server.hpp"
#include "raven_services/diagnostics_service.hpp"
namespace raven {
// One BLE producer sends console replies; quiet event output is pulled only
// when replies and the transport are idle. Neither path uses Critical policy.
class BleConsoleTx final : public runtime::Component {
  public:
    BleConsoleTx(BleSerialServer &serial, diagnostics::Journal &journal);
    void on_message(const runtime::Message &message, uint64_t now) override;
    void on_tick(uint64_t now) override;

  private:
    void synchronize();
    bool send(uint32_t generation, const char *bytes, size_t size);
    BleSerialServer &serial_;
    diagnostics::Journal &journal_;
    uint32_t generation_{};
    uint64_t cursor_{1}, next_event_{};
    bool watch_{true};
};
// NimBLE callbacks copy packets into the transport queue. This task parses text
// and delivers commands; no console business logic runs on the NimBLE host.
class BleConsoleRx {
  public:
    BleConsoleRx(BleSerialServer &serial, runtime::Dispatcher &dispatcher, uint8_t destination)
        : serial_(serial), dispatcher_(dispatcher), destination_(destination) {}
    void start();
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }

  private:
    void run();
    BleSerialServer &serial_;
    runtime::Dispatcher &dispatcher_;
    uint8_t destination_;
    services::ConsoleParser parser_;
    TaskHandle_t handle_{};
};
} // namespace raven
