#include "lwip/sockets.h"
#include "raven_platform/tcp_network.hpp"
#undef recv
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <unistd.h>

#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)
namespace {
struct Read {
    int count, error;
    uint64_t elapsed;
};
std::deque<Read> reads;
unsigned calls{};
uint64_t now{100};
void (*rx_entry)(void *){};
void *rx_context{};
struct StopRx {};
struct Connection {
    int client{-1}, accepted{-1};
    Connection() {
        const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        CHECK(listener >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        CHECK(bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        socklen_t length = sizeof(address);
        CHECK(getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length) == 0);
        CHECK(listen(listener, 1) == 0);
        client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        CHECK(connect(client, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0);
        accepted = accept(listener, nullptr, nullptr);
        CHECK(accepted >= 0);
        close(listener);
    }
    ~Connection() { close(client); } // The tested channel owns accepted after attach.
};
} // namespace
ssize_t raven_test_recv(int fd, void *bytes, size_t capacity, int flags) {
    ++calls;
    if (reads.empty())
        return ::recv(fd, bytes, capacity, flags);
    const auto read = reads.front();
    reads.pop_front();
    now += read.elapsed;
    errno = read.error;
    if (read.count > 0) {
        CHECK(size_t(read.count) <= capacity);
        std::memset(bytes, 0, size_t(read.count)); // First byte of a length prefix.
    }
    return read.count;
}
int64_t esp_timer_get_time() {
    return int64_t(now * 1000);
}
BaseType_t xTaskCreate(void (*entry)(void *), const char *, uint32_t, void *context, UBaseType_t,
                       TaskHandle_t *handle) {
    rx_entry = entry;
    rx_context = context;
    *handle = context;
    return pdPASS;
}
void vTaskDelay(TickType_t) {
    if (reads.empty())
        throw StopRx{}; // Stop only at an observable idle/yield point, never inside recv.
}
namespace raven {
void OwnerTask::start() {}
} // namespace raven

void receive_outcomes() {
    raven::comms::Session session;
    session.configure(0, 0);
    raven::TcpChannel channel(session);
    Connection connection;
    CHECK(channel.attach(connection.accepted, now));
    const auto key = session.snapshot().key;
    uint8_t bytes[8]{};
    // Also exercise an actual SO_RCVTIMEO expiration on the POSIX socket.
    CHECK(channel.receive(key, bytes, sizeof(bytes)) == raven::TcpChannel::receive_retry);
    for (int error : {EAGAIN, EWOULDBLOCK, EINTR}) {
        reads.push_back({-1, error, 0});
        CHECK(channel.receive(key, bytes, sizeof(bytes)) == raven::TcpChannel::receive_retry);
    }
    for (int error : {ECONNRESET, ECONNABORTED, ENOTCONN, EBADF}) {
        reads.push_back({-1, error, 0});
        CHECK(channel.receive(key, bytes, sizeof(bytes)) == raven::TcpChannel::receive_failed);
    }
    reads.push_back({0, 0, 0});
    CHECK(channel.receive(key, bytes, sizeof(bytes)) == 0);
    CHECK(session.snapshot().io_users == 0);
    session.close(key, raven::comms::Failure::TransportLost);
    const auto before = calls;
    CHECK(channel.receive(key, bytes, sizeof(bytes)) == raven::TcpChannel::receive_failed);
    CHECK(calls == before);
    {
        raven::comms::Session::Guard guard(session);
        guard.state.peer_retired = true;
    }
    CHECK(channel.close_if_quiet());
}
void receive_lifecycle() {
    for (unsigned path = 0; path < 4; ++path) {
        raven::comms::Session session;
        session.configure(0, 0);
        raven::runtime::Dispatcher dispatcher;
        raven::comms::CommunicationPort port(dispatcher, &session, 1, 1, 2);
        raven::TcpEndpoint endpoint(session, port);
        Connection first;
        CHECK(endpoint.attach(first.accepted, now));
        const auto key = session.snapshot().key;
        endpoint.start();
        calls = 0;
        if (path == 0)
            reads = {{-1, EAGAIN, 10}, {-1, EINTR, 10}, {-1, ECONNRESET, 10}};
        else if (path == 1)
            reads = {{0, 0, 0}};
        else if (path == 2)
            reads = {{1, 0, 0}, {-1, ECONNRESET, 10}};
        else
            reads = {{1, 0, 0}, {-1, EAGAIN, 1000}};
        const auto expected_calls = reads.size();
        try {
            rx_entry(rx_context);
        } catch (StopRx &) {
        }
        const auto closed = session.snapshot();
        CHECK(reads.empty() && calls == expected_calls);
        CHECK(closed.phase == raven::comms::Phase::Closing && closed.key == key);
        CHECK(closed.failure ==
              (path == 3 ? raven::comms::Failure::Protocol : raven::comms::Failure::TransportLost));
        CHECK(closed.io_users == 0 && !endpoint.reap()); // Peer retirement is still required.
        {
            raven::comms::Session::Guard guard(session);
            guard.state.peer_retired = true;
        }
        CHECK(endpoint.reap() && session.snapshot().phase == raven::comms::Phase::Vacant);
        Connection next;
        CHECK(endpoint.attach(next.accepted, now));
        const auto reused = session.snapshot().key;
        CHECK(reused != key);
        session.close(key, raven::comms::Failure::TransportLost);
        CHECK(session.active(reused)); // Old failures cannot close the replacement.
        session.close(reused, raven::comms::Failure::TransportLost);
        {
            raven::comms::Session::Guard guard(session);
            guard.state.peer_retired = true;
        }
        CHECK(endpoint.reap());
    }
}
int main() {
    receive_outcomes();
    receive_lifecycle();
    std::puts("PASS 2 TCP receive scenarios");
}
