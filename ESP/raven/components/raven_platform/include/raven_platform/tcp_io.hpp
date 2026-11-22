#pragma once
#include <cstdint>
#include <cstddef>

namespace raven::tcp {
// Bounded socket helpers used by the command/telemetry workers and board tests.
void close_socket(int fd);
void timeouts(int fd, int milliseconds);
bool send_all(int fd, const void* buffer, size_t size);
bool receive_all(int fd, void* buffer, size_t size);
int listen_at(uint16_t port);
int connect_to(const char* host, uint16_t port);
bool command_options(int fd);
} // namespace raven::tcp
