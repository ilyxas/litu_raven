#pragma once
#include "raven_diagnostics/journal.hpp"
namespace raven {
// Capture bounded WARN/ERROR records while preserving the SDK UART log sink.
// Journal consumers perform BLE/network output later in their own contexts.
void capture_system_log(diagnostics::Journal &journal, icd::DeviceId device, icd::BootId boot);
uint32_t system_log_drops();
} // namespace raven
