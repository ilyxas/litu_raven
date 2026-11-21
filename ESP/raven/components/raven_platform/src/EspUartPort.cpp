#include "raven_platform/EspUartPort.hpp"

#include "esp_err.h"

#include <algorithm>

namespace raven {

EspUartPort::EspUartPort(const Config& config)
    : config_(config)
{
}

EspUartPort::~EspUartPort()
{
    close();
}

bool EspUartPort::open()
{
    if (open_) {
        return true;
    }

    uart_config_t uart_config{};
    uart_config.baud_rate = config_.baud_rate;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    if (uart_param_config(config_.port, &uart_config) != ESP_OK) {
        return false;
    }

    if (uart_set_pin(config_.port,
                     config_.tx_gpio,
                     config_.rx_gpio,
                     UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE) != ESP_OK) {
        return false;
    }

    if (uart_driver_install(config_.port,
                            static_cast<int>(config_.rx_buffer_size),
                            static_cast<int>(config_.tx_buffer_size),
                            0,
                            nullptr,
                            0) != ESP_OK) {
        return false;
    }

    open_ = true;
    flush_input();
    return true;
}

void EspUartPort::close()
{
    if (!open_) {
        return;
    }

    uart_driver_delete(config_.port);
    open_ = false;
}

bool EspUartPort::is_open() const
{
    return open_;
}

ssize_t EspUartPort::write(const void* data, size_t size)
{
    if (!open_ || data == nullptr || size == 0) {
        return -1;
    }

    return uart_write_bytes(config_.port,
                            static_cast<const char*>(data),
                            size);
}

ssize_t EspUartPort::read(void* data, size_t capacity, uint32_t timeout_ms)
{
    if (!open_ || data == nullptr || capacity == 0) {
        return -1;
    }

    return uart_read_bytes(config_.port,
                           data,
                           static_cast<uint32_t>(capacity),
                           to_ticks(timeout_ms));
}

bool EspUartPort::wait_tx_idle(uint32_t timeout_ms)
{
    return open_ && uart_wait_tx_done(config_.port, to_ticks(timeout_ms)) == ESP_OK;
}

void EspUartPort::flush_input()
{
    if (open_) {
        uart_flush_input(config_.port);
    }
}

TickType_t EspUartPort::to_ticks(uint32_t timeout_ms)
{
    if (timeout_ms == 0) {
        return 0;
    }

    return std::max<TickType_t>(1, pdMS_TO_TICKS(timeout_ms));
}

} // namespace raven
