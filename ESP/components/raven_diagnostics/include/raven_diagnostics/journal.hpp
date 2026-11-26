#pragma once
#include "raven_icd/device_protocol.hpp"
#include <cstddef>
#include <mutex>

namespace raven::diagnostics {
enum class Event : uint16_t {
    Boot = 1,
    LinkOpened,
    PeerReady,
    PeerNotReady,
    LinkLost,
    ExchangeFailed,
    IdentityConflict,
    ProbeCompleted,
    RemoteEvent,
    SystemWarning,
    SystemError
};
struct Record {
    uint64_t sequence{}, received_ms{}, source_ms{}, device{}, boot{};
    uint32_t repeats{};
    Event code{};
    uint8_t severity{}, size{};
    char text[80]{};
};
static_assert(sizeof(Record) == 128);
// Fixed journal supplied by composition. Writers only copy bounded values; UART,
// BLE and network consumers pull independently. No logging I/O under this lock.
class Journal {
  public:
    Journal(Record *records, size_t capacity) : records_(records), capacity_(capacity) {}
    uint64_t append(Event code, uint64_t now, icd::DeviceId device, icd::BootId boot, const char *text,
                    uint8_t severity = 1, uint64_t source_ms = 0);
    bool read(uint64_t sequence, Record &record) const;
    uint64_t latest() const;
    uint64_t oldest() const;
    size_t capacity() const { return capacity_; }

  private:
    Record *records_;
    size_t capacity_;
    mutable std::mutex mutex_;
    uint64_t sequence_{};
};
} // namespace raven::diagnostics
