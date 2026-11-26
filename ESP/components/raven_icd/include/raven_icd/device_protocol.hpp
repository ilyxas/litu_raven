#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace raven::icd {
// Device language, independent of Raven inboxes, RTOS and byte transports.
constexpr uint8_t version = 1;
constexpr size_t max_packet = 64, header_size = 8;
using DeviceId = uint64_t;
using BootId = uint64_t;
enum class Type : uint16_t {
    HelloRequest = 1,
    HelloResponse,
    KeepAliveRequest,
    KeepAliveResponse,
    Receipt,
    Diagnostic
};
enum class Role : uint8_t { MissionComputer = 1, Node = 2 };
enum class Readiness : uint8_t { NotReady, Ready, Rejected };
enum class Reason : uint16_t { None, Initializing, IdentityConflict, Incompatible, InvalidIdentity };
enum class Admission : uint8_t { Accepted, Full, Invalid, Unknown };
struct DeviceDescriptor {
    DeviceId device{};
    BootId boot{};
    uint16_t capabilities{};
    uint8_t protocol{version};
    Role role{Role::Node};
    Readiness readiness{Readiness::NotReady};
    uint8_t flags{};
    Reason reason{Reason::Initializing};
    char name[16]{};
};
struct Hello {
    DeviceDescriptor device;
    uint64_t uptime_ms{};
};
struct Heartbeat {
    uint64_t origin_ms{}, uptime_ms{};
    Reason reason{};
    Readiness readiness{};
    uint8_t reserved{};
};
struct Receipt {
    Type request_type{};
    Admission admission{};
    uint8_t reserved{};
};
struct Diagnostic {
    uint64_t source_ms{};
    uint32_t sequence{};
    uint16_t code{};
    uint8_t severity{}, size{};
    char text[32]{};
};
struct Body {
    // Owned representation, never a union of implicitly active C++ objects.
    // Messages cross inboxes by memcpy; materialize the selected type as a value.
    template <class T> T get() const {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(bytes));
        T value{};
        std::memcpy(&value, bytes.data(), sizeof(value));
        return value;
    }
    template <class T> void set(const T &value) {
        static_assert(std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(bytes));
        bytes.fill(0);
        std::memcpy(bytes.data(), &value, sizeof(value));
    }

  private:
    alignas(uint64_t) std::array<uint8_t, sizeof(Hello)> bytes{};
};
struct Packet {
    Type type{};
    bool requires_ack{};
    uint32_t exchange{};
    Body body{};
};
enum class Decode { Ok, Invalid, UnsupportedVersion, UnknownType };
bool known(Type type);
bool request(Type type);
size_t encode(const Packet &packet, uint8_t *output, size_t capacity);
Decode decode(const uint8_t *bytes, size_t size, Packet &packet);
static_assert(sizeof(DeviceDescriptor) == 40 && sizeof(Body) == 48);
static_assert(std::is_trivially_copyable_v<Body>);
} // namespace raven::icd
