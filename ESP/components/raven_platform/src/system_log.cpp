#include "raven_platform/system_log.hpp"
#include "esp_log.h"
#include "raven_diagnostics/log_capture.hpp"
#include "raven_platform/clock.hpp"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
namespace raven {
namespace {
diagnostics::Journal *journal{};
icd::DeviceId device{};
icd::BootId boot{};
// The new hook can run before esp_log_set_vprintf returns the old callback.
// Keep serial output usable throughout installation; publish the captured
// callback atomically because log calls run in other tasks.
std::atomic<vprintf_like_t> previous{&std::vprintf};
std::atomic<bool> configured{false};
std::atomic_flag capturing = ATOMIC_FLAG_INIT;
std::atomic<uint32_t> dropped{0};
std::atomic<bool> interrupted{false};
diagnostics::LogCapture capture;
int log_output(const char *format, va_list arguments) {
    const auto output = previous.load(std::memory_order_acquire);
    if (!configured.load(std::memory_order_acquire))
        return output(format, arguments);
    if (capturing.test_and_set()) {
        ++dropped;
        interrupted = true;
        return output(format, arguments);
    }
    if (interrupted.exchange(false))
        capture.reset();
    char text[128]{};
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(text, sizeof(text), format, copy);
    va_end(copy);
    capture.feed(text, now_ms(), *journal, device, boot);
    // Log V1 includes LF in this call; it may have been truncated by formatting.
    if (length >= int(sizeof(text)))
        capture.feed("\n", now_ms(), *journal, device, boot);
    capturing.clear();
    return output(format, arguments);
}
} // namespace
void capture_system_log(diagnostics::Journal &target, icd::DeviceId id, icd::BootId boot_id) {
    journal = &target;
    device = id;
    boot = boot_id;
    configured.store(true, std::memory_order_release);
    const auto output = esp_log_set_vprintf(log_output);
    previous.store(output ? output : &std::vprintf, std::memory_order_release);
}
uint32_t system_log_drops() {
    return dropped.load();
}
} // namespace raven
