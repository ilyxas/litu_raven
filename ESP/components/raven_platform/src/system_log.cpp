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
vprintf_like_t previous{};
std::atomic_flag capturing = ATOMIC_FLAG_INIT;
std::atomic<uint32_t> dropped{0};
std::atomic<bool> interrupted{false};
diagnostics::LogCapture capture;
int log_output(const char *format, va_list arguments) {
    if (capturing.test_and_set()) {
        ++dropped;
        interrupted = true;
        return previous(format, arguments);
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
    return previous(format, arguments);
}
} // namespace
void capture_system_log(diagnostics::Journal &target, icd::DeviceId id, icd::BootId boot_id) {
    journal = &target;
    device = id;
    boot = boot_id;
    previous = esp_log_set_vprintf(log_output);
}
uint32_t system_log_drops() {
    return dropped.load();
}
} // namespace raven
