#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/types.h>

namespace raven {

// Byte-oriented full-duplex serial port.
//
// The interface deliberately uses milliseconds instead of FreeRTOS ticks so
// protocol code can remain independent from the ESP-IDF execution backend.
class ISerialPort {
public:
    virtual ~ISerialPort() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;

    virtual ssize_t write(const void* data, size_t size) = 0;
    virtual ssize_t read(void* data, size_t capacity, uint32_t timeout_ms) = 0;

    virtual bool wait_tx_idle(uint32_t timeout_ms) = 0;
    virtual void flush_input() = 0;
};

} // namespace raven
