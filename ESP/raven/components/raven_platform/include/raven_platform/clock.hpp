#pragma once
#include "esp_timer.h"

namespace raven {
inline uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time()) / 1000; }
} // namespace raven
