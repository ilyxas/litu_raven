#include "raven_gateways/serial_control_protocol.hpp"
#include "raven_contracts/arm_commands.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

namespace raven::demo {
using namespace raven::runtime;
namespace {
const char *status_name(Status value) {
    constexpr const char *names[]{"Unknown", "Running",  "Succeeded", "Superseded",
                                  "Halted",  "TimedOut", "Failed",    "Rejected"};
    return names[static_cast<unsigned>(value)];
}
bool number(const char *text, float &value) {
    if (!text || !*text)
        return false;
    char *end{};
    errno = 0;
    value = std::strtof(text, &end);
    return end != text && *end == '\0' && errno != ERANGE && std::isfinite(value);
}
bool operation_id(const char *text, uint32_t &value) {
    if (!text || !*text)
        return false;
    uint64_t parsed = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9')
            return false;
        parsed = parsed * 10 + unsigned(*p - '0');
        if (parsed > UINT32_MAX)
            return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}
int endpoint(const char *text) {
    if (!text)
        return -1;
    if (!std::strcmp(text, "left"))
        return 0;
    if (!std::strcmp(text, "right"))
        return 1;
    if (!std::strcmp(text, "activity"))
        return 2;
    return -1;
}
} // namespace
uint32_t SerialControlProtocol::next_request() {
    return next_ < 0x80000000u ? next_++ : 0; // Exhaustion fails rather than aliasing a child.
}
void SerialControlProtocol::reset() {
    used_ = 0;
    discard_ = false;
    controls_ = {};
    watching_ = true;
    last_readouts_ = {};
}
SerialControlProtocol::Reply SerialControlProtocol::error(const char *reason) const {
    Reply reply;
    std::snprintf(reply.text.data(), reply.text.size(), "D0:NACK %s\n", reason);
    return reply;
}
SerialControlProtocol::Reply SerialControlProtocol::receipt(uint32_t id, const DeliveryReport &report) const {
    Reply reply;
    std::snprintf(reply.text.data(), reply.text.size(), "D0:%s id=%lu accepted=%u rejected=%u reason=%u\n",
                  report.complete() ? "ACK" : "NACK", static_cast<unsigned long>(id), report.accepted,
                  report.rejected, unsigned(report.reason));
    return reply;
}
SerialControlProtocol::Reply SerialControlProtocol::status(unsigned ep, uint32_t id) const {
    const auto value = states_[ep]->find(id);
    Reply reply;
    std::snprintf(reply.text.data(), reply.text.size(),
                  "D0:STATUS %s id=%lu %s pos=%.1f target=%.1f continuous=%u\n",
                  ep == 0   ? "left"
                  : ep == 1 ? "right"
                            : "activity",
                  static_cast<unsigned long>(value.operation), status_name(value.status),
                  double(value.position), double(value.target), unsigned(value.continuous));
    return reply;
}
SerialControlProtocol::Reply SerialControlProtocol::motion(unsigned arm, float value, bool continuous) {
    if (arm > 1 || !std::isfinite(value) || value < -180 || value > 180)
        return error("target_range -180..180");
    const uint32_t id = next_request();
    if (!id)
        return error("request_ids_exhausted");
    // Pending Control is Unknown until its owner runs; Target must keep its ID.
    if (controls_[arm] && terminal(states_[arm]->find(controls_[arm]).status))
        controls_[arm] = 0;
    DeliveryReport report;
    if (continuous && controls_[arm]) {
        report =
            dispatcher_.deliver(1u << arm, Message::make(Target, id, TargetValue{controls_[arm], value}));
    } else {
        report = dispatcher_.deliver(1u << arm, Message::make(continuous ? Control : Move, id, Goal{value}));
        if (report.complete())
            controls_[arm] = continuous ? id : 0;
    }
    return receipt(id, report);
}
SerialControlProtocol::Reply SerialControlProtocol::halt(uint8_t mask) {
    const auto id = next_request();
    if (!id)
        return error("request_ids_exhausted");
    if (mask & 1)
        controls_[0] = 0;
    if (mask & 2)
        controls_[1] = 0;
    return receipt(id, dispatcher_.deliver(mask, Message::make(Cut, id, Empty{})));
}
SerialControlProtocol::Reply SerialControlProtocol::command(char *text) {
    // Decode the app's text escapes. Embedded control characters are rejected;
    // one console message cannot smuggle a second command past its line boundary.
    char *out = text;
    for (char *p = text; *p; ++p) {
        if (*p == '\\') {
            ++p;
            if (*p != '\\')
                return error("unsupported_console_escape");
        }
        *out++ = *p;
    }
    *out = '\0';
    std::array<char *, 5> words{};
    size_t count = 0;
    for (char *p = text; *p;) {
        while (*p == ' ' || *p == '\t')
            ++p;
        if (!*p)
            break;
        if (count == words.size())
            return error("too_many_arguments");
        words[count++] = p;
        while (*p && *p != ' ' && *p != '\t')
            ++p;
        if (*p)
            *p++ = '\0';
    }
    if (!count)
        return error("empty_command");
    if (!std::strcmp(words[0], "watch") && count == 2) {
        if (std::strcmp(words[1], "on") && std::strcmp(words[1], "off")) return error("watch on|off");
        watching_ = !std::strcmp(words[1], "on");
        last_readouts_ = {};
        Reply reply;
        std::snprintf(reply.text.data(), reply.text.size(), "D0:ACK watch=%s\n", watching_ ? "on" : "off");
        return reply;
    }
    if (!std::strcmp(words[0], "help") && count == 1) {
        Reply reply;
        std::snprintf(reply.text.data(), reply.text.size(),
                      "D0:move/control/target left|right deg; pair L R; halt [left|right|all]; status "
                      "[left|right|activity] [id]; watch on|off; mode left|right polling|callback|silent|stalled|error\n");
        return reply;
    }
    if (!std::strcmp(words[0], "halt") && count <= 2) {
        if (count == 1 || !std::strcmp(words[1], "all"))
            return halt(7);
        const int ep = endpoint(words[1]);
        return ep >= 0 && ep < 2 ? halt(1u << ep) : error("endpoint");
    }
    if (!std::strcmp(words[0], "status") && count <= 3) {
        const int ep = count == 1 ? 0 : endpoint(words[1]);
        uint32_t id = 0;
        if (ep < 0 || (count == 3 && !operation_id(words[2], id)))
            return error("status_arguments");
        return status(unsigned(ep), id);
    }
    if (!std::strcmp(words[0], "pair") && count == 3) {
        PairGoal goal;
        if (!number(words[1], goal.left) || !number(words[2], goal.right) || std::fabs(goal.left) > 180 ||
            std::fabs(goal.right) > 180)
            return error("pair_targets");
        const auto id = next_request();
        if (!id)
            return error("request_ids_exhausted");
        return receipt(id, dispatcher_.deliver(4, Message::make(Pair, id, goal)));
    }
    const int ep = count >= 2 ? endpoint(words[1]) : -1;
    if (ep < 0 || ep > 1 || count != 3)
        return error("command_arguments; type help");
    if (!std::strcmp(words[0], "mode")) {
        constexpr const char *modes[]{"polling", "callback", "silent", "stalled", "error"};
        for (unsigned i = 0; i < 5; ++i)
            if (!std::strcmp(words[2], modes[i])) {
                const auto id = next_request();
                if (!id)
                    return error("request_ids_exhausted");
                return receipt(id, dispatcher_.deliver(
                                       1u << ep, Message::make(Configure, id,
                                                               Configuration{static_cast<DriverMode>(i)})));
            }
        return error("driver_mode");
    }
    float value;
    if (!number(words[2], value))
        return error("number");
    if (!std::strcmp(words[0], "move"))
        return motion(unsigned(ep), value, false);
    if (!std::strcmp(words[0], "control"))
        return motion(unsigned(ep), value, true);
    if (!std::strcmp(words[0], "target")) {
        if (!controls_[ep] || terminal(states_[ep]->find(controls_[ep]).status))
            return error("no_control_session");
        return motion(unsigned(ep), value, true);
    }
    return error("unknown_command; type help");
}
SerialControlProtocol::Reply SerialControlProtocol::handle(char *line) {
    if (!std::strncmp(line, "C:", 2))
        return command(line + 2);
    // Raw lines also work in generic BLE terminals, without the app's C: wrapper.
    if (line[0] >= 'a' && line[0] <= 'z')
        return command(line);
    char *p = line + 1;
    if (*p < '0' || *p > '9')
        return error("channel");
    unsigned ch = 0;
    for (; *p >= '0' && *p <= '9'; ++p) {
        ch = ch * 10 + unsigned(*p - '0');
        if (ch > 19)
            return error("channel");
    }
    if (*p++ != ':')
        return error("separator");
    if (line[0] == 'B' && ch <= 2 && p[0] && !p[1]) {
        if (*p != 'D' && *p != 'R' && *p != 'U')
            return error("button_event");
        if (*p == 'U' || (*p == 'R' && ch != 0)) {
            Reply reply;
            std::strcpy(reply.text.data(), "D0:ACK button_release_or_repeat\n");
            return reply;
        }
        if (ch == 0)
            return halt(7);
        char preset[32];
        std::strcpy(preset, ch == 1 ? "pair 45 -45" : "pair 0 0");
        return command(preset);
    }
    if (ch > 1)
        return error("unmapped_channel");
    float value;
    if (line[0] == 'S' && number(p, value))
        return motion(ch, value, false);
    if (line[0] == 'T' && (!std::strcmp(p, "0") || !std::strcmp(p, "1")))
        return *p == '0' ? halt(1u << ch) : motion(ch, states_[ch]->current().position, true);
    if (line[0] == 'J') {
        char *comma = std::strchr(p, ',');
        if (!comma)
            return error("joystick_format");
        *comma = '\0';
        float magnitude;
        if (!number(p, value) || !number(comma + 1, magnitude) || value < 0 || value > 360 || magnitude < 0 ||
            magnitude > 1)
            return error("joystick_range");
        return motion(ch, 90.0f * magnitude * std::cos(value * 0.01745329252f), true);
    }
    if (line[0] == 'P') {
        if (!p[0] || !std::strchr("UDLRC", p[0]))
            return error("dpad_direction");
        if (p[1] && (p[1] != ',' || !p[2] || !std::strchr("DUR", p[2]) || p[3]))
            return error("dpad_event");
        if (p[1] && p[2] == 'U')
            return halt(1u << ch);
        return motion(ch, p[0] == 'C' ? 0 : (p[0] == 'U' || p[0] == 'R' ? 90 : -90), true);
    }
    return error("unmapped_or_invalid_control");
}
bool SerialControlProtocol::feed(const uint8_t *bytes, size_t size, Emit emit, void *context) {
    for (size_t i = 0; i < size; ++i) {
        const char ch = static_cast<char>(bytes[i]);
        if (ch == '\n') {
            if (!used_ && !discard_)
                continue;
            if (used_ && line_[used_ - 1] == '\r')
                --used_;
            line_[used_] = '\0';
            const auto reply = discard_ ? error("line_too_long_or_invalid") : handle(line_.data());
            used_ = 0;
            discard_ = false;
            if (!emit(context, reply.text.data(), std::strlen(reply.text.data())))
                return false;
        } else if (!discard_) {
            if (used_ == line_.size() - 1 ||
                (static_cast<unsigned char>(ch) < 32 && ch != '\r' && ch != '\t') ||
                static_cast<unsigned char>(ch) > 126)
                discard_ = true;
            else
                line_[used_++] = ch;
        }
    }
    return true;
}
bool SerialControlProtocol::readouts(Emit emit, void *context) {
    if (!watching_) return true;
    const auto changed = [&](size_t slot, const char* text, size_t size) {
        auto& last = last_readouts_[slot];
        if (!std::strcmp(last.data(), text)) return true;
        if (!emit(context, text, size)) return false;
        std::memcpy(last.data(), text, size + 1);
        return true;
    };
    for (unsigned i = 0; i < 3; ++i) {
        const auto value = states_[i]->current();
        char text[160];
        const int size =
            std::snprintf(text, sizeof(text), "D%u:%s id=%lu\n", i + 1, status_name(value.status),
                          static_cast<unsigned long>(value.operation));
        if (!changed(i * 2, text, size_t(size)))
            return false;
        if (i < 2) {
            const int length =
                std::snprintf(text, sizeof(text), "G%u:%.1f\nI%u:%u\n", i, double(value.position), i,
                              unsigned(value.status == Status::Running));
            if (!changed(i * 2 + 1, text, size_t(length)))
                return false;
        }
    }
    constexpr char ready[] = "L0:Raven ready\nI3:1\n";
    return changed(5, ready, sizeof(ready) - 1);
}
} // namespace raven::demo
