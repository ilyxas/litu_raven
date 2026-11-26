#include "raven_diagnostics/log_capture.hpp"
#include <cstring>

namespace raven::diagnostics {
void LogCapture::feed(const char *fragment, uint64_t now, Journal &journal, icd::DeviceId device,
                      icd::BootId boot) {
    for (const char *c = fragment; *c; ++c) {
        if (*c == '\r')
            continue;
        if (*c != '\n') {
            if (used_ + 1 < sizeof(line_))
                line_[used_++] = *c;
            continue;
        }
        line_[used_] = 0;
        if (used_ > 3 && (line_[0] == 'W' || line_[0] == 'E') && line_[1] == ' ') {
            const bool error = line_[0] == 'E';
            const char *body = line_ + 2;
            if (*body == '(') {
                const auto *end = std::strchr(body, ')');
                if (end)
                    body = end + 1;
            }
            while (*body == ' ')
                ++body;
            journal.append(error ? Event::SystemError : Event::SystemWarning, now, device, boot, body,
                           error ? 3 : 2);
        }
        used_ = 0;
    }
}
} // namespace raven::diagnostics
