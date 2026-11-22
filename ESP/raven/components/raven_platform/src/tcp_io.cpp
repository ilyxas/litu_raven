#include "raven_platform/tcp_io.hpp"
#include "raven_platform/clock.hpp"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <cerrno>

namespace raven::tcp {
void close_socket(int fd) { if (fd >= 0) { shutdown(fd, SHUT_RDWR); close(fd); } }
void timeouts(int fd, int milliseconds) {
    const timeval timeout{milliseconds / 1000, (milliseconds % 1000) * 1000};
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    const int no_delay = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(no_delay));
}
bool send_all(int fd, const void* buffer, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    const uint64_t deadline = now_ms() + 500;
    while (size && now_ms() < deadline) {
        const int sent = send(fd, bytes, size, 0);
        if (sent <= 0) return false;
        bytes += sent; size -= static_cast<size_t>(sent);
    }
    return size == 0;
}
bool receive_all(int fd, void* buffer, size_t size) {
    auto* bytes = static_cast<uint8_t*>(buffer);
    const uint64_t deadline = now_ms() + 2000;
    while (size && now_ms() < deadline) {
        const int count = recv(fd, bytes, size, 0);
        if (count <= 0) return false;
        bytes += count; size -= static_cast<size_t>(count);
    }
    return size == 0;
}
int listen_at(uint16_t port) {
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_ANY); address.sin_port = htons(port);
    if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(fd, 1)) {
        close_socket(fd); return -1;
    }
    return fd;
}
int connect_to(const char* host, uint16_t port) {
    // Numeric IPv4 only: bounded connect, no blocking DNS in the telemetry worker.
    sockaddr_in address{};
    address.sin_family = AF_INET; address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) return -1;
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    const int result = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    if (result && errno != EINPROGRESS) { close_socket(fd); return -1; }
    fd_set writes; FD_ZERO(&writes); FD_SET(fd, &writes);
    timeval wait{0, 300000};
    int error = 0; socklen_t length = sizeof(error);
    if (select(fd + 1, nullptr, &writes, nullptr, &wait) <= 0 ||
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) || error) { close_socket(fd); return -1; }
    fcntl(fd, F_SETFL, flags);
    timeouts(fd, 200);
    return fd;
}
bool command_options(int fd) {
    timeouts(fd, 200);
    const int enabled = 1, idle = 3, interval = 1, probes = 3;
    // Abortive close prevents replacement from waiting for an unreachable peer
    // to acknowledge old outbound data. Accepted operations remain in services.
    const linger discard_pending{1, 0};
    return setsockopt(fd, SOL_SOCKET, SO_LINGER, &discard_pending, sizeof(discard_pending)) == 0 &&
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled)) == 0 &&
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) == 0 &&
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) == 0 &&
        setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(probes)) == 0;
}
} // namespace raven::tcp
