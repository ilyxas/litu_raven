#pragma once
#include "raven_diagnostics/journal.hpp"

namespace raven::diagnostics {
// ESP-IDF Log V2 emits prefix, body and newline in separate vprintf calls.
// Assemble a bounded line before capturing WARN/ERROR, stripping the SDK timestamp
// so identical warnings can coalesce. The platform serializes feed/reset.
class LogCapture {
  public:
    void feed(const char *fragment, uint64_t now, Journal &journal, icd::DeviceId device, icd::BootId boot);
    void reset() { used_ = 0; }

  private:
    char line_[128]{};
    size_t used_{};
};
} // namespace raven::diagnostics
