#pragma once

#include "raven_platform/ISerialPort.hpp"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include <cstddef>
#include <cstdint>

namespace raven {

class EspUartPort final : public ISerialPort {
public:
    struct Config {
        uart_port_t port;
        int tx_gpio;
        int rx_gpio;
        int baud_rate{115200};
        size_t rx_buffer_size{2048};
        size_t tx_buffer_size{0};
    };

    explicit EspUartPort(const Config& config);
    ~EspUartPort() override;

    EspUartPort(const EspUartPort&) = delete;
    EspUartPort& operator=(const EspUartPort&) = delete;

    bool open() override;
    void close() override;
    bool is_open() const override;

    ssize_t write(const void* data, size_t size) override;
    ssize_t read(void* data, size_t capacity, uint32_t timeout_ms) override;

    bool wait_tx_idle(uint32_t timeout_ms) override;
    void flush_input() override;

private:
    static TickType_t to_ticks(uint32_t timeout_ms);

    Config config_;
    bool open_{false};
};

} // namespace raven
