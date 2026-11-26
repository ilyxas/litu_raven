// Test firmware for one spare node. Exercises the real MC server over Wi-Fi;
// synthetic IDs exist only here, never in production device configuration.
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "raven_comms/length_framing.hpp"
#include "raven_platform/clock.hpp"
#include "raven_platform/wifi_network.hpp"
#include <array>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace raven {
namespace {
constexpr size_t probe_count = 6;
enum class Behavior { Normal, BadVersion, Partial, InvalidLength, SlowReader };
struct Client {
    int fd{-1};
    uint64_t identity{}, boot{1};
    unsigned hellos{}, heartbeats{};
    bool remote_closed{};
    Behavior behavior{Behavior::Normal};
    comms::LengthFrameReader reader;
    void close() {
        if (fd >= 0) {
            linger immediate{1, 0};
            setsockopt(fd, SOL_SOCKET, SO_LINGER, &immediate, sizeof(immediate));
            ::close(fd);
        }
        fd = -1;
    }
    void connect(uint32_t gateway, uint16_t port, uint64_t id, Behavior mode = Behavior::Normal) {
        close();
        identity = id;
        ++boot;
        hellos = heartbeats = 0;
        remote_closed = false;
        behavior = mode;
        reader.reset();
        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(fd >= 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = gateway;
        const int result = ::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
        assert(result == 0);
        int yes = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
        assert(fcntl(fd, F_SETFL, O_NONBLOCK) == 0);
    }
    bool send(const icd::Packet &packet) {
        uint8_t payload[icd::max_packet]{}, wire[icd::max_packet + 2]{};
        const auto n = icd::encode(packet, payload, sizeof(payload));
        assert(n);
        const auto length = comms::length_frame(payload, n, wire, sizeof(wire));
        if (behavior == Behavior::BadVersion)
            wire[2] = 99;
        const auto written = ::send(fd, wire, length, MSG_DONTWAIT);
        if (written == int(length))
            return true;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return false;
        remote_closed = written < 0;
        close();
        return false;
    }
    static bool packet(void *context, const uint8_t *bytes, size_t size, uint64_t now) {
        auto &self = *static_cast<Client *>(context);
        icd::Packet input{};
        assert(icd::decode(bytes, size, input) == icd::Decode::Ok);
        if (input.type == icd::Type::HelloRequest) {
            if (self.behavior == Behavior::Partial) {
                const uint8_t first = 0;
                ::send(self.fd, &first, 1, 0);
                return true;
            }
            if (self.behavior == Behavior::InvalidLength) {
                const uint8_t bad[] = {0xff, 0xff};
                ::send(self.fd, bad, 2, 0);
                return true;
            }
            if (input.requires_ack && self.behavior == Behavior::Normal) {
                icd::Packet ack{};
                ack.type = icd::Type::Receipt;
                ack.exchange = input.exchange;
                ack.body.set(icd::Receipt{input.type, icd::Admission::Accepted, 0});
                if (!self.send(ack))
                    return false;
            }
            icd::Packet response{};
            response.type = icd::Type::HelloResponse;
            response.exchange = input.exchange;
            icd::Hello hello{};
            hello.uptime_ms = now;
            hello.device.device = self.identity;
            hello.device.boot = self.boot;
            hello.device.role = icd::Role::Node;
            hello.device.readiness = icd::Readiness::Ready;
            response.body.set(hello);
            if (!self.send(response))
                return false;
            ++self.hellos;
        } else if (input.type == icd::Type::KeepAliveRequest) {
            icd::Packet response{};
            response.type = icd::Type::KeepAliveResponse;
            response.exchange = input.exchange;
            response.body.set(icd::Heartbeat{input.body.get<icd::Heartbeat>().origin_ms, now,
                                             icd::Reason::None, icd::Readiness::Ready, 0});
            if (!self.send(response))
                return false;
            ++self.heartbeats;
        }
        return true;
    }
    void poll() {
        if (fd < 0 || behavior == Behavior::SlowReader)
            return;
        uint8_t bytes[128]{};
        const int count = recv(fd, bytes, sizeof(bytes), MSG_DONTWAIT);
        if (count > 0) {
            if (!reader.feed(bytes, size_t(count), now_ms(), packet, this))
                close();
        } else if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            remote_closed = true;
            close();
        }
    }
};
std::array<Client, probe_count> clients;
void run_for(uint64_t duration) {
    const auto end = now_ms() + duration;
    while (now_ms() < end) {
        for (auto &client : clients)
            client.poll();
        vTaskDelay(1);
    }
}
void healthy(size_t count) {
    for (size_t i = 0; i < count; ++i)
        assert(clients[i].fd >= 0 && clients[i].hellos && clients[i].heartbeats);
}
} // namespace
void run_tcp_board_probe(WifiNetwork &wifi, uint16_t port) {
    while (!wifi.ready())
        vTaskDelay(pdMS_TO_TICKS(100));
    // Allow the MC to retire previous physical sessions before filling its profile.
    vTaskDelay(pdMS_TO_TICKS(10000));
    constexpr uint64_t first_id = 0xF000000000000001ULL;
    const auto gateway = wifi.gateway();
    for (size_t i = 0; i < clients.size(); ++i)
        clients[i].connect(gateway, port, first_id + i);
    run_for(20000);
    healthy(probe_count);
    ESP_LOGI("BoardProbe", "PASS six full-duplex sessions / 20s keepalive");
    auto &victim = clients.back();
    victim.close();
    run_for(1500);
    healthy(probe_count - 1);
    victim.connect(gateway, port, first_id + probe_count - 1);
    run_for(2500);
    healthy(probe_count);
    ESP_LOGI("BoardProbe", "PASS independent close and reconnect");
    victim.close();
    run_for(500);
    victim.connect(gateway, port, first_id);
    run_for(2500);
    assert(victim.fd < 0 && victim.remote_closed);
    healthy(probe_count - 1);
    ESP_LOGI("BoardProbe", "PASS duplicate identity rejected without takeover");
    for (const auto mode : {Behavior::BadVersion, Behavior::InvalidLength, Behavior::Partial}) {
        victim.connect(gateway, port, first_id + probe_count - 1, mode);
        run_for(1800);
        assert(victim.fd < 0 && victim.remote_closed);
        healthy(probe_count - 1);
    }
    ESP_LOGI("BoardProbe", "PASS bad version, invalid length, absolute partial-frame timeout");
    victim.connect(gateway, port, first_id + probe_count - 1);
    run_for(1500);
    assert(victim.fd >= 0 && victim.hellos);
    victim.behavior = Behavior::SlowReader;
    icd::Packet request{};
    request.type = icd::Type::HelloRequest;
    request.exchange = 10000;
    icd::Hello hello{};
    hello.device.device = victim.identity;
    hello.device.boot = victim.boot;
    hello.device.role = icd::Role::Node;
    hello.device.readiness = icd::Readiness::Ready;
    request.body.set(hello);
    const auto end = now_ms() + 2500;
    while (now_ms() < end) {
        if (victim.fd >= 0) {
            ++request.exchange;
            victim.send(request);
        }
        run_for(10);
    }
    victim.behavior = Behavior::Normal;
    run_for(2000);
    assert(victim.fd < 0 && victim.remote_closed);
    healthy(probe_count - 1);
    ESP_LOGI("BoardProbe", "PASS non-reading peer contained; other sessions alive");
    victim.connect(gateway, port, first_id + probe_count - 1);
    run_for(2500);
    healthy(probe_count);
    ESP_LOGI("BoardProbe", "PASS recovery after malformed and congested streams");
    for (auto &client : clients)
        client.close();
    ESP_LOGI("BoardProbe", "COMPLETE");
}
} // namespace raven
