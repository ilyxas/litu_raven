#pragma once

namespace raven {
// Starts the development AP or the configured station. Call once at bootstrap.
void start_wifi(const char* ssid, const char* password);
} // namespace raven
