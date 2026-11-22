#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace raven::demo {
// Demo operation view; position/target are arm-specific, not a universal task schema.
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
} // namespace raven::demo
