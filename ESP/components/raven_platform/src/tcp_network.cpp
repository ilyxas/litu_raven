#include "raven_platform/tcp_network.hpp"
#include "esp_check.h"
#include "esp_random.h"
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "raven_platform/clock.hpp"
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace raven {
static_assert(icd::max_packet + 2 <= TCP_SNDLOWAT,
              "Revisit TCP write-space/memory contract for larger frames");
namespace {
comms::Clock clock_source() {
    return {[](void *) { return now_ms(); }, nullptr};
}
void abort_close(int fd) {
    const linger immediate{1, 0};
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &immediate, sizeof(immediate));
    close(fd);
}
bool retryable() {
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}
} // namespace
bool TcpChannel::attach(int fd, uint64_t now) {
    if (descriptor_ != -1 || session_.snapshot().phase != comms::Phase::Vacant)
        return false;
    const timeval receive_timeout{0, 100000};
    const int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &receive_timeout, sizeof(receive_timeout)) ||
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)))
        return false;
    // Listening/connect sockets are nonblocking; each accepted RX uses blocking
    // recv with a bounded timeout. TX uses MSG_DONTWAIT and an absolute deadline.
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags & ~O_NONBLOCK))
        return false;
    interrupted_ = false;
    descriptor_ = fd;
    if (!session_.open(now)) {
        descriptor_ = -1;
        return false;
    }
    return true;
}
comms::Failure TcpChannel::write_packet(comms::SessionKey route, const uint8_t *packet, size_t packet_size,
                                        uint64_t deadline) {
    uint8_t bytes[icd::max_packet + 2];
    const auto size = comms::length_frame(packet, packet_size, bytes, sizeof(bytes));
    if (!size)
        return comms::Failure::Protocol;
    if (!session_.acquire_io(route))
        return comms::Failure::TransportLost;
    const int fd = descriptor_.load();
    size_t offset = 0;
    comms::Failure failure = comms::Failure::None;
    while (offset < size) {
        const auto now = now_ms();
        if (fd < 0 || !session_.active(route)) {
            failure = comms::Failure::TransportLost;
            break;
        }
        if (now >= deadline) {
            failure = comms::Failure::SendDeadline;
            break;
        }
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        const auto remaining = deadline - now;
        timeval timeout{long(remaining / 1000), long((remaining % 1000) * 1000)};
        const int ready = select(fd + 1, nullptr, &writable, nullptr, &timeout);
        if (ready == 0) {
            failure = comms::Failure::SendDeadline;
            break;
        }
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            failure = comms::Failure::WriteError;
            break;
        }
        const int written = send(fd, bytes + offset, size - offset, MSG_DONTWAIT);
        if (written > 0)
            offset += size_t(written);
        else if (written < 0 && retryable())
            vTaskDelay(1);
        else {
            failure = comms::Failure::WriteError;
            break;
        }
    }
    session_.release_io();
    return failure;
}
int TcpChannel::receive(comms::SessionKey route, uint8_t *bytes, size_t capacity) {
    if (!session_.acquire_io(route))
        return -2;
    const int fd = descriptor_.load();
    const int count = fd < 0 ? 0 : recv(fd, bytes, capacity, 0);
    const bool again = count < 0 && retryable();
    session_.release_io();
    return again ? -1 : count;
}
void TcpChannel::interrupt() {
    if (interrupted_)
        return;
    const int fd = descriptor_.load();
    if (fd >= 0)
        shutdown(fd, SHUT_RDWR);
    interrupted_ = true;
}
bool TcpChannel::close_if_quiet() {
    const auto state = session_.snapshot();
    if (state.phase != comms::Phase::Closing || state.io_users || !state.peer_retired)
        return false;
    const int fd = descriptor_.exchange(-1);
    if (fd >= 0)
        abort_close(fd);
    return session_.retire();
}
TcpEndpoint::TcpEndpoint(comms::Session &session, comms::CommunicationPort &port)
    : session_(session), port_(port), channel_(session), tx_(session, channel_, clock_source()),
      owner_(tx_, "tcp_tx", 4) {}
void TcpEndpoint::start() {
    owner_.start();
    const auto result = xTaskCreate([](void *p) { static_cast<TcpEndpoint *>(p)->receive_loop(); }, "tcp_rx",
                                    3072, this, 4, &rx_handle_);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
unsigned TcpEndpoint::rx_stack_free() const {
    return unsigned(uxTaskGetStackHighWaterMark(rx_handle_));
}
bool TcpEndpoint::packet(void *ctx, const uint8_t *bytes, size_t size, uint64_t now) {
    auto &self = *static_cast<TcpEndpoint *>(ctx);
    icd::Packet value{};
    if (icd::decode(bytes, size, value) != icd::Decode::Ok)
        return false;
    self.port_.receive(self.reading_, value, now);
    return self.session_.active(self.reading_);
}
void TcpEndpoint::receive_loop() {
    for (;;) {
        const auto state = session_.snapshot();
        if (state.phase != comms::Phase::Open) {
            reader_.reset();
            reading_ = {};
            rx_idle_ = true;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        rx_idle_ = false;
        if (reading_ != state.key) {
            reader_.reset();
            reading_ = state.key;
        }
        const int count = channel_.receive(reading_, receive_buffer_.data(), receive_buffer_.size());
        if (count == -1) {
            if (reader_.expired(now_ms()))
                session_.close(reading_, comms::Failure::Protocol);
        } else if (count <= 0)
            session_.close(reading_, comms::Failure::TransportLost);
        else if (!reader_.feed(receive_buffer_.data(), size_t(count), now_ms(), packet, this))
            session_.close(reading_, comms::Failure::Protocol);
        // Sustained ingress must not prevent idle and lower-priority diagnostics.
        if (count > 0)
            vTaskDelay(1);
    }
}
bool TcpEndpoint::reap() {
    if (session_.snapshot().phase != comms::Phase::Closing)
        return false;
    channel_.interrupt();
    return rx_idle_ && !tx_.inbox.pending() && channel_.close_if_quiet();
}
void TcpManager::start() {
    const auto result = xTaskCreate([](void *p) { static_cast<TcpManager *>(p)->run(); }, "tcp_manager", 3072,
                                    this, 4, &handle_);
    ESP_ERROR_CHECK(result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
int TcpManager::connect_to_gateway() {
    const int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
        return -1;
    if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
        abort_close(fd);
        return -1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = wifi_.gateway();
    const int connected = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (connected != 0 && errno != EINPROGRESS) {
        abort_close(fd);
        return -1;
    }
    if (connected != 0) {
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(fd, &writable);
        timeval timeout{1, 0};
        int error = 0;
        socklen_t length = sizeof(error);
        if (select(fd + 1, nullptr, &writable, nullptr, &timeout) <= 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) || error) {
            abort_close(fd);
            return -1;
        }
    }
    return fd;
}
void TcpManager::run() {
    int listener = -1;
    uint64_t next_connect = 0;
    unsigned retry = 0;
    constexpr unsigned retry_ms[] = {250, 500, 1000, 2000, 5000};
    for (;;) {
        for (size_t i = 0; i < count_; ++i) {
            auto &endpoint = *endpoints_[i];
            auto state = endpoint.session().snapshot();
            if (!wifi_.ready() && state.phase == comms::Phase::Open)
                endpoint.session().close(state.key, comms::Failure::TransportLost);
            if (endpoint.reap() && !server_) {
                // A connected but rejected/broken peer must not create a tight
                // reconnect loop. Backoff applies after a live session too.
                next_connect = now_ms() + retry_ms[std::min<unsigned>(retry++, 4)] + esp_random() % 101;
            }
        }
        if (wifi_.ready() && server_) {
            if (listener < 0) {
                listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (listener >= 0) {
                    int yes = 1;
                    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
                    sockaddr_in address{};
                    address.sin_family = AF_INET;
                    address.sin_port = htons(port_);
                    address.sin_addr.s_addr = htonl(INADDR_ANY);
                    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) ||
                        listen(listener, int(count_)) || fcntl(listener, F_SETFL, O_NONBLOCK)) {
                        abort_close(listener);
                        listener = -1;
                    }
                }
            }
            if (listener >= 0) {
                const int fd = accept(listener, nullptr, nullptr);
                if (fd >= 0) {
                    bool attached = false;
                    for (size_t i = 0; i < count_ && !attached; ++i)
                        attached = endpoints_[i]->attach(fd, now_ms());
                    if (!attached)
                        abort_close(fd);
                }
            }
        } else if (wifi_.ready() && !server_ &&
                   endpoints_[0]->session().snapshot().phase == comms::Phase::Vacant &&
                   now_ms() >= next_connect) {
            const int fd = connect_to_gateway();
            if (fd >= 0 && endpoints_[0]->attach(fd, now_ms()))
                retry = 0;
            else {
                if (fd >= 0)
                    abort_close(fd);
                next_connect = now_ms() + retry_ms[std::min<unsigned>(retry++, 4)] + esp_random() % 101;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // namespace raven
