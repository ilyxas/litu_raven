#pragma once
#include "raven_comms/session.hpp"
#include "raven_core/component.hpp"

namespace raven::comms {
struct Clock {
    using Read = uint64_t (*)(void *);
    Read read{};
    void *context{};
    uint64_t now() const { return read(context); }
};
// Owns framing and delivery of one encoded ICD packet within an absolute local
// deadline. TCP owns an I/O lease; UART may use different frame boundaries.
// Success means local transport acceptance, never remote semantic completion.
class PacketWriter {
  public:
    virtual ~PacketWriter() = default;
    virtual Failure write_packet(SessionKey route, const uint8_t *bytes, size_t size, uint64_t deadline) = 0;
};
class TxComponent final : public runtime::Component {
  public:
    TxComponent(Session &session, PacketWriter &writer, Clock clock);
    void on_message(const runtime::Message &message, uint64_t now) override;
    void on_tick(uint64_t) override {}

  private:
    Session &session_;
    PacketWriter &writer_;
    Clock clock_;
    std::array<uint8_t, icd::max_packet> bytes_{};
};
} // namespace raven::comms
