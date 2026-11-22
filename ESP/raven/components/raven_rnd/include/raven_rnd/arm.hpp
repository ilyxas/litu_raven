#pragma once

#include "raven_runtime/runtime.hpp"

namespace raven::rnd {
using namespace raven::runtime;
enum MessageId : uint16_t { Move = 1, Control, Target, Cut, Configure, DriverDone, Pair, CancelChildren };
constexpr uint32_t first_child_id = 0x80000000u;
// This prototype's single Activity owns the monotonically allocated child range.
struct ChildCancellation { uint32_t through{}; };
struct Goal { float target{}; };
struct TargetValue { uint32_t operation{}; float target{}; };
struct Empty {};
enum class DriverMode : uint8_t { Polling, Callback, Silent, Stalled, Error };
struct Configuration { DriverMode mode{}; };
struct Completion { uint64_t generation{}; float position{}; };
enum class Status : uint8_t { Unknown, Running, Succeeded, Superseded, Halted, TimedOut, Failed, Rejected };
inline bool terminal(Status status) { return status != Status::Unknown && status != Status::Running; }
struct Snapshot {
    uint32_t operation{};
    Status status{Status::Unknown};
    float position{}, target{};
    bool continuous{};
    uint64_t changed_ms{};
};

// One owner writes; readers poll copies. Finite, explicitly documented retention.
class OperationState {
public:
    static constexpr size_t history_capacity = 16;
    static constexpr size_t retention_capacity = 4;
    // Reserve before dispatch. The state must outlive this move-only handle.
    class ResultLease {
    public:
        ResultLease() = default;
        ~ResultLease();
        ResultLease(const ResultLease&) = delete;
        ResultLease& operator=(const ResultLease&) = delete;
        ResultLease(ResultLease&& other) noexcept;
        ResultLease& operator=(ResultLease&& other) noexcept;
        explicit operator bool() const { return owner_ != nullptr; }
        Snapshot snapshot() const;
        void reset();
    private:
        friend class OperationState;
        ResultLease(OperationState& owner, size_t slot) : owner_(&owner), slot_(slot) {}
        OperationState* owner_{};
        size_t slot_{};
    };
    Snapshot current() const;
    Snapshot find(uint32_t id) const;
    ResultLease retain(uint32_t id);
    void publish(const Snapshot& value);
private:
    Snapshot find_locked(uint32_t id) const;
    struct Retained { bool used{}; Snapshot value{}; };
    mutable std::mutex mutex_;
    Snapshot current_{};
    std::array<Snapshot, history_capacity> history_{};
    std::array<Retained, retention_capacity> retained_{};
    size_t next_{};
};

struct DriverStatus { float position{}; bool ready{}, reached{}, fault{}; };
class IArmDriver {
public:
    using Callback = void (*)(void*, const Completion&);
    virtual ~IArmDriver() = default;
    virtual void begin(float target, uint64_t generation, uint64_t now) = 0;
    virtual void stop() = 0;
    virtual void advance(uint64_t now) = 0;
    virtual DriverStatus read() const = 0;
    virtual void configure(DriverMode mode) = 0;
    virtual void set_callback(Callback callback, void* context) = 0;
};

class SimArmDriver final : public IArmDriver {
public:
    void begin(float target, uint64_t generation, uint64_t now) override;
    void stop() override { moving_ = false; }
    void advance(uint64_t now) override;
    DriverStatus read() const override;
    void configure(DriverMode mode) override { mode_ = mode; }
    void set_callback(Callback callback, void* context) override { callback_ = callback; context_ = context; }
    uint64_t generation() const { return generation_; }
private:
    DriverMode mode_{DriverMode::Polling};
    float position_{}, target_{};
    uint64_t previous_{}, generation_{};
    bool moving_{};
    Callback callback_{};
    void* context_{};
};

class ArmService final : public Component {
public:
    explicit ArmService(IArmDriver& driver);
    OperationState state;
    void on_message(const Message& message, uint64_t now) override;
    void on_tick(uint64_t now) override;
private:
    static void callback(void* context, const Completion& completion);
    void start(uint32_t id, float target, bool continuous, uint64_t now);
    void retarget(float target, uint64_t now);
    void finish(Status status, uint64_t now);
    void reject(uint32_t id, float target, Status status, uint64_t now);
    DriverStatus sample_motion(uint64_t now);
    bool child_cancelled(uint32_t id) const { return id >= first_child_id && id <= child_cutoff_; }
    IArmDriver& driver_;
    Snapshot active_{};
    uint64_t generation_{}, deadline_{}, cutoff_{};
    TargetValue latest_target_{};
    uint64_t latest_target_sequence_{};
    uint32_t child_cutoff_{};
    bool moving_{};
};

struct PairGoal { float left{}, right{}; };
// Orchestration observes child statuses. It owns no actuator deadlines.
class DualArmActivity final : public Component {
public:
    DualArmActivity(Dispatcher& dispatcher, OperationState& left, OperationState& right);
    OperationState state;
    void on_message(const Message& message, uint64_t now) override;
    void on_tick(uint64_t now) override;
private:
    void release_children();
    Dispatcher& dispatcher_;
    OperationState& left_;
    OperationState& right_;
    OperationState::ResultLease left_result_, right_result_;
    Status left_outcome_{Status::Unknown}, right_outcome_{Status::Unknown};
    Snapshot active_{};
    uint32_t left_id_{}, right_id_{}, next_id_{first_child_id};
    uint64_t cutoff_{};
};
} // namespace raven::rnd
