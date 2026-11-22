#include "runtime_self_test.hpp"
#include "raven_gateways/frame_reader.hpp"
#include "raven_services/arm_service.hpp"
#include "raven_activities/dual_arm_activity.hpp"
#include "raven_platform/sim_arm_driver.hpp"
#include "raven_core/executor.hpp"
#include "raven_gateways/serial_control_protocol.hpp"
#include <cmath>

namespace raven::demo {
namespace {
#define CHECK(condition) do { if (!(condition)) return false; } while (false)
struct Fixture {
    SimArmDriver ld, rd;
    ArmService left{ld}, right{rd};
    Dispatcher dispatch;
    DualArmActivity activity{dispatch, left.state, right.state};
    Executor le{left, 10}, re{right, 10}, ae{activity, 10};
    Protocol protocol{dispatch, left.state, right.state, activity.state};
    uint64_t now{};
    Fixture() { dispatch.bind(0, left.inbox); dispatch.bind(1, right.inbox); dispatch.bind(2, activity.inbox); }
    void advance(unsigned ms) {
        for (unsigned i = 0; i < ms; i += 10) { now += 10; ae.step(now); le.step(now); re.step(now); }
    }
    void move(uint8_t mask, uint32_t id, float value) { dispatch.deliver(mask, Message::make(Move, id, Goal{value})); }
};

bool receipt_before_execution() {
    Fixture f;
    uint8_t payload[9]{};
    wire::put32(payload, 42);
    float angle = 90;
    std::memcpy(payload + 5, &angle, 4);
    auto response = f.protocol.handle(wire::move, payload, sizeof(payload));
    CHECK(response.id == wire::ack && response.payload[7] == 1);
    CHECK(f.left.state.find(42).status == Status::Unknown);
    f.advance(10);
    CHECK(f.left.state.find(42).status == Status::Running);
    f.advance(1200);
    CHECK(f.left.state.find(42).status == Status::Succeeded);
    return true;
}
bool independent_retarget() {
    Fixture f;
    f.move(1, 1, 90); f.move(2, 2, -90);
    f.advance(200);
    f.move(1, 3, -45);
    f.advance(20);
    CHECK(f.left.state.find(1).status == Status::Superseded);
    CHECK(f.right.state.find(2).status == Status::Running);
    f.advance(1500);
    CHECK(f.left.state.find(3).status == Status::Succeeded);
    CHECK(f.right.state.find(2).status == Status::Succeeded);
    return true;
}
bool critical_under_overload() {
    Fixture f;
    for (uint32_t i = 1; i <= Inbox::capacity; ++i) f.move(1, i, 90);
    CHECK(f.dispatch.deliver(1, Message::make(Move, 30, Goal{90})).reason == Delivery::Full);
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(f.dispatch.deliver(1, Message::make(Cut, 100 + i, Empty{})).complete());
    CHECK(f.left.inbox.pending() == Inbox::capacity + 1);
    f.advance(200);
    for (uint32_t i = 1; i <= Inbox::capacity; ++i) CHECK(f.left.state.find(i).status == Status::Halted);
    CHECK(f.left.state.current().position == 0);
    f.move(1, 50, 10); // No invented reset/reboot requirement.
    f.advance(500);
    CHECK(f.left.state.find(50).status == Status::Succeeded);
    return true;
}
bool stream_latest_value() {
    Fixture f;
    f.dispatch.deliver(1, Message::make(Control, 1, Goal{}));
    f.advance(20);
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(f.dispatch.deliver(1, Message::make(Target, i + 2, TargetValue{1, float(i % 91)})).complete());
    CHECK(f.left.inbox.pending() == 1);
    f.advance(1500);
    CHECK(f.left.state.current().status == Status::Running);
    CHECK(f.left.state.current().target == float(999 % 91));
    CHECK(std::fabs(f.left.state.current().position - f.left.state.current().target) < 0.01f);
    f.dispatch.deliver(1, Message::make(Cut, 2000, Empty{}));
    f.advance(20);
    CHECK(f.left.state.find(1).status == Status::Halted);
    return true;
}
bool callback_and_stale_result() {
    Fixture f;
    f.ld.configure(DriverMode::Callback);
    f.move(1, 1, 20); f.advance(20);
    const auto old_generation = f.ld.generation();
    f.move(1, 2, -20); f.advance(20);
    f.left.inbox.post(Message::make(DriverDone, 0, Completion{old_generation, 20}));
    f.advance(20);
    CHECK(f.left.state.find(2).status == Status::Running);
    f.advance(1000);
    CHECK(f.left.state.find(2).status == Status::Succeeded);
    CHECK(f.left.state.current().position == -20);
    return true;
}
bool target_before_control_execution() {
    Fixture f;
    f.dispatch.deliver(1, Message::make(Configure, 1, Configuration{DriverMode::Polling}));
    f.advance(10); // The next ordinary turn prefers the latest-value lane.
    f.dispatch.deliver(1, Message::make(Control, 2, Goal{}));
    f.dispatch.deliver(1, Message::make(Target, 3, TargetValue{2, 45}));
    f.advance(1000);
    CHECK(f.left.state.current().operation == 2 && f.left.state.current().target == 45);
    CHECK(f.left.state.current().position == 45 && f.left.state.current().status == Status::Running);
    return true;
}
bool executor_owned_failure() {
    for (auto mode : {DriverMode::Silent, DriverMode::Stalled, DriverMode::Error}) {
        Fixture f;
        f.ld.configure(mode);
        f.move(1, 1, 90); f.advance(2000);
        CHECK(f.left.state.find(1).status == (mode == DriverMode::Error ? Status::Failed : Status::TimedOut));
    }
    return true;
}
bool activity_polls_children() {
    Fixture f;
    f.dispatch.deliver(4, Message::make(Pair, 1, PairGoal{45, -90}));
    f.advance(1500);
    CHECK(f.activity.state.find(1).status == Status::Succeeded);
    f.ld.configure(DriverMode::Silent);
    f.dispatch.deliver(4, Message::make(Pair, 2, PairGoal{90, 0}));
    // Activity has no child-operation timeout of its own.
    for (uint64_t t = 2000; t < 10000; t += 10) f.ae.step(t);
    CHECK(f.activity.state.find(2).status == Status::Running);
    f.now = 10000;
    f.advance(2000);
    CHECK(f.left.state.current().status == Status::TimedOut);
    CHECK(f.activity.state.find(2).status == Status::Failed);
    return true;
}
bool partial_delivery() {
    Fixture f;
    for (uint32_t i = 1; i <= Inbox::capacity; ++i) f.move(1, i, 90);
    auto report = f.dispatch.deliver(3, Message::make(Move, 100, Goal{}));
    CHECK(!report.complete() && report.accepted == 2 && report.rejected == 1 && report.reason == Delivery::Full);
    CHECK(f.right.inbox.pending() == 1);
    CHECK(f.dispatch.deliver(8, Message::make(Cut, 0, Empty{})).reason == Delivery::UnknownMessage);
    return true;
}
bool pair_remembers_observed_result() {
    Fixture f;
    f.dispatch.deliver(4, Message::make(Pair, 1, PairGoal{0, 180}));
    f.advance(50);
    CHECK(f.left.state.find(first_child_id).status == Status::Succeeded);
    for (uint32_t id = 2; id < 18; ++id) { f.move(1, id, 0); f.advance(30); }
    CHECK(f.left.state.find(first_child_id).status == Status::Unknown);
    f.advance(2200);
    CHECK(f.activity.state.find(1).status == Status::Succeeded);
    return true;
}
bool pair_retains_unobserved_result() {
    Fixture f;
    f.dispatch.deliver(4, Message::make(Pair, 1, PairGoal{0, 180}));
    f.ae.step(0); // Dispatch children, then withhold all Activity polls.
    const auto arms_only = [&](unsigned ms) {
        for (unsigned i = 0; i < ms; i += 10) { f.now += 10; f.le.step(f.now); f.re.step(f.now); }
    };
    arms_only(50);
    for (uint32_t id = 2; id < 18; ++id) { f.move(1, id, 0); arms_only(30); }
    CHECK(f.left.state.find(first_child_id).status == Status::Unknown);
    f.advance(2200);
    CHECK(f.activity.state.find(1).status == Status::Succeeded);
    return true;
}
bool retarget_between_ticks() {
    for (const auto mode : {DriverMode::Polling, DriverMode::Callback}) {
        Fixture f;
        f.ld.configure(mode);
        f.dispatch.deliver(1, Message::make(Control, 1, Goal{90}));
        f.le.step(0);
        for (uint64_t t = 10; t <= 1000; t += 10) {
            const float target = (t / 10) % 2 ? 91.0f : 90.0f;
            f.dispatch.deliver(1, Message::make(Target, uint32_t(t), TargetValue{1, target}));
            f.le.step(t - 1); // Nine milliseconds of old motion must survive.
            f.le.step(t);
        }
        CHECK(std::fabs(f.left.state.current().position - 90) < 0.01f);
        CHECK(f.left.state.current().status == Status::Running);
    }
    Fixture f;
    f.ld.configure(DriverMode::Callback);
    f.dispatch.deliver(1, Message::make(Control, 1, Goal{0.45f}));
    f.le.step(0);
    f.dispatch.deliver(1, Message::make(Target, 2, TargetValue{1, 90}));
    f.le.step(9); // Sampling the old motion generates its completion callback.
    f.le.step(10); // That queued callback must not overwrite the new position.
    CHECK(std::fabs(f.left.state.current().position - 0.54f) < 0.001f);
    CHECK(f.left.state.current().target == 90 && f.left.state.current().status == Status::Running);
    return true;
}
bool result_retention_lifecycle() {
    Fixture f;
    std::array<OperationState::ResultLease, OperationState::retention_capacity> held;
    for (size_t i = 0; i < held.size(); ++i) {
        held[i] = f.left.state.retain(uint32_t(100 + i));
        CHECK(bool(held[i]));
    }
    f.dispatch.deliver(4, Message::make(Pair, 1, PairGoal{180, -180}));
    f.advance(20);
    CHECK(f.activity.state.find(1).status == Status::Rejected);
    CHECK(f.left.state.current().status == Status::Unknown && f.right.state.current().status == Status::Unknown);
    for (auto& lease : held) lease.reset();
    // Repeated replacement must free the old wait before reserving a new one.
    for (uint32_t id = 2; id < 10; ++id) {
        f.dispatch.deliver(4, Message::make(Pair, id, PairGoal{180, -180}));
        f.advance(20);
        CHECK(f.activity.state.find(id).status == Status::Running);
    }
    f.dispatch.deliver(7, Message::make(Cut, 10, Empty{}));
    f.advance(30);
    CHECK(f.activity.state.find(9).status == Status::Halted);
    const auto all_slots_available = [&held](OperationState& state) {
        for (size_t i = 0; i < held.size(); ++i) {
            held[i] = state.retain(uint32_t(200 + i));
            if (!held[i]) return false;
        }
        for (auto& lease : held) lease.reset();
        return true;
    };
    CHECK(all_slots_available(f.left.state) && all_slots_available(f.right.state));
    for (uint32_t id = 300; id < 300 + Inbox::capacity; ++id) f.move(1, id, 0);
    f.dispatch.deliver(4, Message::make(Pair, 400, PairGoal{0, 0}));
    f.ae.step(f.now + 10); // Left rejects admission; right accepts its child.
    CHECK(f.activity.state.find(400).status == Status::Rejected);
    CHECK(all_slots_available(f.left.state) && all_slots_available(f.right.state));
    return true;
}
bool cut_during_pair_dispatch() {
    for (bool newer_external_goal : {false, true}) {
        Fixture f;
        f.dispatch.deliver(4, Message::make(Pair, 1, PairGoal{90, -90}));
        Message pair;
        CHECK(f.activity.inbox.take_normal(pair));
        // Model a cut arriving after Pair was dequeued but before its handler
        // publishes child goals. Arm-local sequence barriers alone miss these.
        CHECK(f.dispatch.deliver(7, Message::make(Cut, 2, Empty{})).complete());
        f.le.step(10); f.re.step(10);
        f.activity.on_message(pair, 10);
        f.le.step(20); f.re.step(20);
        CHECK(f.left.state.current().status == Status::Running);
        if (newer_external_goal) {
            f.move(1, 3, 45);
            f.le.step(30);
        }
        f.now = 30;
        f.advance(20); // Activity handles Cut and cancels only its own children.
        CHECK(f.activity.state.find(1).status == Status::Halted);
        CHECK(f.right.state.current().status == Status::Halted);
        CHECK(f.left.state.current().status == (newer_external_goal ? Status::Running : Status::Halted));
        f.advance(1200);
        if (newer_external_goal) CHECK(f.left.state.find(3).status == Status::Succeeded);
        f.dispatch.deliver(4, Message::make(Pair, 4, PairGoal{0, 0}));
        f.advance(1500);
        CHECK(f.activity.state.find(4).status == Status::Succeeded);
    }
    return true;
}
struct Capture {
    unsigned count{};
    Response last;
    static bool receive(void* ctx, const Response& response) {
        auto& self = *static_cast<Capture*>(ctx); ++self.count; self.last = response; return true;
    }
};
bool framing_and_validation() {
    Fixture f;
    FrameReader reader(f.protocol);
    uint8_t frames[22]{};
    // Two cut frames. A fragmented header followed by coalesced frames.
    for (unsigned offset : {0u, 11u}) {
        frames[offset] = 4; frames[offset + 1] = 0x11;
        wire::put32(frames + offset + 2, 5); wire::put32(frames + offset + 6, offset + 1);
        frames[offset + 10] = 3;
    }
    Capture capture;
    for (unsigned i = 0; i < 3; ++i) CHECK(reader.feed(frames + i, 1, Capture::receive, &capture));
    CHECK(reader.feed(frames + 3, sizeof(frames) - 3, Capture::receive, &capture));
    CHECK(capture.count == 2 && capture.last.id == wire::ack);
    uint8_t oversized[6]{}; wire::put32(oversized + 2, 0xffffffff);
    CHECK(!reader.feed(oversized, 6, Capture::receive, &capture));
    uint8_t invalid[4]{1, 0, 0, 0};
    CHECK(f.protocol.handle(wire::move, invalid, 4).id == wire::nack);
    return true;
}
bool history_and_duplicate() {
    Fixture f;
    f.move(1, 1, 10); f.advance(1000);
    f.move(1, 1, 80); f.advance(20);
    CHECK(f.left.state.current().target == 10);
    for (uint32_t i = 2; i <= 20; ++i) { f.move(1, i, 10); f.advance(30); }
    CHECK(f.left.state.find(1).status == Status::Unknown);
    CHECK(f.left.state.find(20).status == Status::Succeeded);
    return true;
}
bool tick_under_flood() {
    struct Probe final : Component {
        unsigned ticks{}, commands{}, critical{};
        Probe() { inbox.subscribe(1, sizeof(Empty), Policy::Ordered); inbox.subscribe(2, sizeof(Empty), Policy::Critical); }
        void on_message(const Message& m, uint64_t) override { if (m.id == 2) ++critical; else ++commands; }
        void on_tick(uint64_t) override { ++ticks; }
    } probe;
    Executor executor(probe, 10);
    for (unsigned i = 0; i < 1000; ++i) {
        probe.inbox.post(Message::make(1, i, Empty{}));
        probe.inbox.post(Message::make(2, i, Empty{}));
        executor.step(i);
    }
    CHECK(probe.ticks == 100 && probe.commands > 0 && probe.critical == 1000);
    return true;
}
bool payload_ownership() {
    Inbox inbox;
    CHECK(inbox.subscribe(1, sizeof(Goal), Policy::Ordered));
    auto message = Message::make(1, 1, Goal{45});
    CHECK(inbox.post(message) == Delivery::Accepted);
    message.payload.fill(0);
    Message received; Goal goal;
    CHECK(inbox.take_normal(received) && received.read(goal) && goal.target == 45);
    return true;
}
struct SerialFixture : Fixture {
    SerialControlProtocol text{dispatch, left.state, right.state, activity.state};
    std::array<char, 256> reply{};
    unsigned replies{};
    static bool emit(void* ctx, const char* bytes, size_t size) {
        auto& self = *static_cast<SerialFixture*>(ctx);
        if (size >= self.reply.size()) return false;
        std::memcpy(self.reply.data(), bytes, size); self.reply[size] = '\0';
        ++self.replies;
        return true;
    }
    bool send(const char* bytes) {
        return text.feed(reinterpret_cast<const uint8_t*>(bytes), std::strlen(bytes), emit, this);
    }
    bool contains(const char* value) const { return std::strstr(reply.data(), value) != nullptr; }
};
bool serial_fragmented_console() {
    SerialFixture f;
    for (const char ch : "C:move left 45") {
        if (ch) CHECK(f.text.feed(reinterpret_cast<const uint8_t*>(&ch), 1, SerialFixture::emit, &f));
    }
    CHECK(f.replies == 0 && f.left.inbox.pending() == 0);
    CHECK(f.send("\r\nC:move right -30\n"));
    CHECK(f.replies == 2 && f.contains("ACK"));
    CHECK(f.left.state.current().status == Status::Unknown); // Admission precedes owners.
    f.advance(1000);
    CHECK(f.left.state.current().status == Status::Succeeded && f.left.state.current().position == 45);
    CHECK(f.right.state.current().status == Status::Succeeded && f.right.state.current().position == -30);
    CHECK(f.send("C:status left\n") && f.contains("Succeeded"));
    CHECK(f.send("status right\n") && f.contains("pos=-30.0"));
    return true;
}
bool serial_invalid_and_resync() {
    SerialFixture f;
    for (const auto line : {"S0:nan\n", "S0:181\n", "J0:361,1\n", "J0:0,1.1\n", "B0:X\n",
                            "S19:42\n", "P0:U,Z\n", "C:move left 45 extra\n", "C:halt\\nmove left 10\n"}) {
        CHECK(f.send(line) && f.contains("NACK"));
        CHECK(f.left.inbox.pending() == 0 && f.right.inbox.pending() == 0 && f.activity.inbox.pending() == 0);
    }
    std::array<uint8_t, 200> too_long; too_long.fill('x');
    CHECK(f.text.feed(too_long.data(), too_long.size(), SerialFixture::emit, &f));
    CHECK(f.send("\n") && f.contains("NACK"));
    const uint8_t embedded_zero[]{'C', ':', 'h', 'a', 'l', 't', 0, '\n'};
    CHECK(f.text.feed(embedded_zero, sizeof(embedded_zero), SerialFixture::emit, &f) && f.contains("NACK"));
    CHECK(f.send("C:move left 30\n") && f.contains("ACK"));
    f.advance(1000);
    CHECK(f.left.state.current().position == 30);
    return true;
}
bool serial_controls_and_halt() {
    SerialFixture f;
    CHECK(f.send("J0:0,1\nJ1:180,1\n"));
    f.advance(200);
    CHECK(f.left.state.current().continuous && f.right.state.current().continuous);
    CHECK(f.left.state.current().position > 0 && f.right.state.current().position < 0);
    for (unsigned i = 0; i < 100; ++i) CHECK(f.send("J0:0,0.5\n"));
    CHECK(f.left.inbox.pending() <= 1); // Latest target lane, bounded under stream.
    CHECK(f.send("B0:D\n") && f.contains("ACK"));
    f.advance(20);
    CHECK(f.left.state.current().status == Status::Halted && f.right.state.current().status == Status::Halted);
    CHECK(f.send("B0:U\n")); f.advance(20);
    CHECK(f.left.state.current().status == Status::Halted); // Release cannot restart.
    CHECK(f.send("P1:U,D\n")); f.advance(100);
    CHECK(f.right.state.current().status == Status::Running);
    CHECK(f.send("P1:U,U\n")); f.advance(20);
    CHECK(f.right.state.current().status == Status::Halted);
    CHECK(f.send("T0:1\nJ0:0,0\n")); f.advance(500);
    CHECK(f.left.state.current().continuous && f.left.state.current().position == 0);
    CHECK(f.send("T0:0\n")); f.advance(20);
    CHECK(f.left.state.current().status == Status::Halted);
    return true;
}
bool serial_reconnect_and_receipts() {
    SerialFixture f;
    CHECK(f.send("C:control left 90\n")); f.advance(50);
    const auto old = f.left.state.current().operation;
    CHECK(f.send("C:move right "));
    f.text.reset(); // New connection cannot finish the previous connection's command.
    CHECK(f.send("45\n") && f.contains("NACK"));
    CHECK(f.right.inbox.pending() == 0 && f.left.state.current().status == Status::Running);
    CHECK(f.send("C:control left -45\n")); f.advance(50);
    CHECK(f.left.state.current().operation != old && f.left.state.find(old).status == Status::Superseded);
    for (size_t i = 0; i < Inbox::capacity; ++i) CHECK(f.send("S0:10\n") && f.contains("D0:ACK"));
    CHECK(f.send("S0:20\n") && f.contains("D0:NACK") && f.contains("rejected=1"));
    CHECK(f.send("C:halt left\n") && f.contains("D0:ACK"));
    f.advance(200);
    CHECK(f.left.state.current().status == Status::Halted);
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.contains("I3:1"));
    return true;
}
bool serial_quiet_readouts() {
    SerialFixture f;
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == 6);
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == 6); // Unchanged view is silent.
    CHECK(f.send("watch off\n") && !f.text.watching());
    const auto before = f.replies;
    CHECK(f.send("move left 45\n") && f.contains("D0:ACK"));
    f.advance(100);
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == before + 1);
    CHECK(f.send("status left\n") && f.contains("Running")); // Explicit queries still reply.
    CHECK(f.send("watch on\n") && f.text.watching());
    const auto resumed = f.replies;
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == resumed + 6);
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == resumed + 6);
    f.advance(100);
    CHECK(f.text.readouts(SerialFixture::emit, &f) && f.replies == resumed + 7); // Only left gauge changed.
    f.text.reset();
    CHECK(f.text.watching());
    return true;
}
}
bool run_self_tests(TestLog log) {
    struct Test { const char* name; bool (*run)(); };
    const Test tests[] = {
        {"receipt_before_execution", receipt_before_execution},
        {"independent_retarget", independent_retarget},
        {"critical_under_overload", critical_under_overload},
        {"stream_latest_value", stream_latest_value},
        {"callback_and_stale_result", callback_and_stale_result},
        {"target_before_control_execution", target_before_control_execution},
        {"executor_owned_failure", executor_owned_failure},
        {"activity_polls_children", activity_polls_children},
        {"partial_delivery", partial_delivery},
        {"pair_remembers_observed_result", pair_remembers_observed_result},
        {"pair_retains_unobserved_result", pair_retains_unobserved_result},
        {"retarget_between_ticks", retarget_between_ticks},
        {"result_retention_lifecycle", result_retention_lifecycle},
        {"cut_during_pair_dispatch", cut_during_pair_dispatch},
        {"framing_and_validation", framing_and_validation},
        {"history_and_duplicate", history_and_duplicate},
        {"tick_under_flood", tick_under_flood},
        {"payload_ownership", payload_ownership},
        {"serial_fragmented_console", serial_fragmented_console},
        {"serial_invalid_and_resync", serial_invalid_and_resync},
        {"serial_controls_and_halt", serial_controls_and_halt},
        {"serial_reconnect_and_receipts", serial_reconnect_and_receipts},
        {"serial_quiet_readouts", serial_quiet_readouts},
    };
    bool all = true;
    for (const auto& test : tests) { const bool ok = test.run(); if (log) log(test.name, ok); all &= ok; }
    return all;
}
} // namespace raven::demo
