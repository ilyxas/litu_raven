#pragma once
#include <cstdint>

namespace raven::demo {
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

struct PairGoal { float left{}, right{}; };
} // namespace raven::demo
