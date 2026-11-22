#pragma once
#include "raven_core/dispatcher.hpp"
#include "raven_state/operation_state.hpp"
#include <array>

namespace raven::demo {
// Bluetooth Serial Connect's newline ASCII boundary and this demo's channel map.
// One gateway task calls feed/reset; all actuator changes go through Dispatcher.
// Emit receives a complete owned-for-the-call line. False ends the session;
// already admitted operations are never rolled back by a transport failure.
class SerialControlProtocol {
  public:
    using Emit = bool (*)(void *, const char *, size_t);
    SerialControlProtocol(runtime::Dispatcher &dispatcher, const OperationState &left,
                          const OperationState &right, const OperationState &activity)
        : dispatcher_(dispatcher), states_{&left, &right, &activity} {}
    bool feed(const uint8_t *bytes, size_t size, Emit emit, void *context);
    void reset(); // Disconnect discards incomplete input, not service operations.
    bool readouts(Emit emit, void *context);
    bool watching() const { return watching_; }

  private:
    struct Reply {
        std::array<char, 192> text{};
    };
    Reply handle(char *line);
    Reply command(char *text);
    Reply receipt(uint32_t request, const runtime::DeliveryReport &report) const;
    Reply error(const char *reason) const;
    Reply status(unsigned endpoint, uint32_t operation) const;
    Reply motion(unsigned arm, float target, bool continuous);
    Reply halt(uint8_t mask);
    uint32_t next_request();
    runtime::Dispatcher &dispatcher_;
    std::array<const OperationState *, 3> states_;
    std::array<uint32_t, 2> controls_{};
    // Separate from binary callers (<0x40000000), legacy iOS (0x40000000)
    // and activity children (>=0x80000000). Never reset/reuse on reconnect.
    uint32_t next_{0x60000000u};
    std::array<char, 128> line_{};
    size_t used_{};
    bool discard_{};
    bool watching_{true};
    std::array<std::array<char, 160>, 6> last_readouts_{};
};
} // namespace raven::demo
