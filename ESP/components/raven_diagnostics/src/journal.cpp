#include "raven_diagnostics/journal.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
namespace raven::diagnostics {
uint64_t Journal::append(Event code, uint64_t now, icd::DeviceId device, icd::BootId boot, const char *text,
                         uint8_t severity, uint64_t source_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    assert(capacity_);
    const auto length = text ? std::min<size_t>(std::strlen(text), sizeof(Record::text) - 1) : 0;
    if (sequence_) {
        auto &last = records_[(sequence_ - 1) % capacity_];
        if (last.code == code && last.device == device && last.boot == boot && last.size == length &&
            now - last.received_ms < 5000 && (!length || std::memcmp(last.text, text, length) == 0)) {
            ++last.repeats;
            return sequence_;
        }
    }
    auto &r = records_[sequence_ % capacity_];
    r = {};
    r.sequence = ++sequence_;
    r.received_ms = now;
    r.source_ms = source_ms;
    r.device = device;
    r.boot = boot;
    r.repeats = 1;
    r.code = code;
    r.severity = severity;
    r.size = uint8_t(length);
    if (length)
        std::memcpy(r.text, text, length);
    return sequence_;
}
bool Journal::read(uint64_t sequence, Record &record) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!sequence || sequence > sequence_ || sequence_ - sequence >= capacity_)
        return false;
    record = records_[(sequence - 1) % capacity_];
    return record.sequence == sequence;
}
uint64_t Journal::latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sequence_;
}
uint64_t Journal::oldest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sequence_ > capacity_ ? sequence_ - capacity_ + 1 : 1;
}
} // namespace raven::diagnostics
