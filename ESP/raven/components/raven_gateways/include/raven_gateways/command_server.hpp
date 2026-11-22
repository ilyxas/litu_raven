#pragma once
#include "raven_gateways/command_protocol.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>

namespace raven::demo {
// Owns the binary TCP session and parser. Newest connection replaces the old
// transport; accepted operations continue in their services. Start once.
class CommandServer {
public:
    CommandServer(Protocol& protocol, uint16_t port) : protocol_(protocol), port_(port) {}
    void start();
    bool ready() const { return ready_; }
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }
private:
    void run();
    Protocol& protocol_;
    uint16_t port_;
    std::atomic<bool> ready_{false};
    TaskHandle_t handle_{};
};
} // namespace raven::demo
