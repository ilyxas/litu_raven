#pragma once
#include "raven_comms/length_framing.hpp"
#include "raven_comms/tx_component.hpp"
#include "raven_platform/owner_task.hpp"
#include "raven_platform/wifi_network.hpp"
#include <atomic>

namespace raven {
// Full-duplex resource shared by RX and its single TX writer. The manager alone
// installs/closes descriptors; I/O leases prevent reuse while a syscall is live.
class TcpChannel final : public comms::PacketWriter {
  public:
    explicit TcpChannel(comms::Session &session) : session_(session) {}
    // Positive byte count, zero EOF, or one of these distinct error outcomes.
    static constexpr int receive_retry = -1, receive_failed = -2;
    bool attach(int descriptor, uint64_t now);
    comms::Failure write_packet(comms::SessionKey route, const uint8_t *packet, size_t packet_size,
                                uint64_t deadline) override;
    int receive(comms::SessionKey route, uint8_t *bytes, size_t capacity);
    void interrupt();
    bool close_if_quiet();

  private:
    comms::Session &session_;
    std::atomic<int> descriptor_{-1};
    bool interrupted_{}; // manager context only
};

// Per-channel execution contexts. RX never waits for service execution. Session
// count and task allocation belong to the app profile, not this implementation.
class TcpEndpoint {
  public:
    TcpEndpoint(comms::Session &session, comms::CommunicationPort &port);
    void start();
    bool attach(int descriptor, uint64_t now) { return channel_.attach(descriptor, now); }
    bool reap();
    unsigned rx_stack_free() const;
    unsigned tx_stack_free() const { return owner_.stack_free(); }
    runtime::Inbox &inbox() { return tx_.inbox; }
    comms::Session &session() { return session_; }

  private:
    void receive_loop();
    static bool packet(void *context, const uint8_t *data, size_t size, uint64_t now);
    comms::Session &session_;
    comms::CommunicationPort &port_;
    TcpChannel channel_;
    comms::TxComponent tx_;
    OwnerTask owner_;
    TaskHandle_t rx_handle_{};
    std::atomic<bool> rx_idle_{true};
    comms::LengthFrameReader reader_;
    comms::SessionKey reading_{};
    std::array<uint8_t, icd::max_packet + 2> receive_buffer_{};
};

class TcpManager {
  public:
    TcpManager(WifiNetwork &wifi, TcpEndpoint **endpoints, size_t count, bool server, uint16_t port)
        : wifi_(wifi), endpoints_(endpoints), count_(count), server_(server), port_(port) {}
    void start();
    unsigned stack_free() const { return unsigned(uxTaskGetStackHighWaterMark(handle_)); }

  private:
    void run();
    int connect_to_gateway();
    WifiNetwork &wifi_;
    TcpEndpoint **endpoints_;
    size_t count_;
    bool server_;
    uint16_t port_;
    TaskHandle_t handle_{};
};
} // namespace raven
