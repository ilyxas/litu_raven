#pragma once
#include "raven_core/component.hpp"
#include "raven_state/sensor_state.hpp"

namespace raven::demo {
// Independent simulated acquisition owner; schedule at 50 ms in composition.
// The temperature is mock data, never a hardware reading.
class SimSensorService final : public runtime::Component {
public:
    SensorState state;
    void on_message(const runtime::Message&, uint64_t) override {}
    void on_tick(uint64_t now) override { state.publish({now, 22.0f + float((now / 100) % 10) / 10.0f}); }
};
} // namespace raven::demo
