#include "raven_services/diagnostics_service.hpp"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace raven::services {
ConsoleRequest ConsoleParser::parse() {
    ConsoleRequest result{};
    result.generation = generation_;
    result.request = ++request_;
    if (overflow_)
        return result;
    line_[used_] = 0;
    char *text = line_.data();
    if (std::strncmp(text, "C:", 2) == 0)
        text += 2;
    while (*text == ' ')
        ++text;
    auto length = std::strlen(text);
    while (length && text[length - 1] == ' ')
        text[--length] = 0;
    if (!std::strcmp(text, "help"))
        result.command = ConsoleCommand::Help;
    else if (!std::strcmp(text, "status"))
        result.command = ConsoleCommand::Status;
    else if (!std::strcmp(text, "peers"))
        result.command = ConsoleCommand::Peers;
    else if (!std::strcmp(text, "stats"))
        result.command = ConsoleCommand::Stats;
    else if (!std::strcmp(text, "watch on"))
        result.command = ConsoleCommand::WatchOn;
    else if (!std::strcmp(text, "watch off"))
        result.command = ConsoleCommand::WatchOff;
    else if (!std::strcmp(text, "logs")) {
        result.command = ConsoleCommand::Logs;
        result.argument = 10;
    } else {
        int base = 16;
        char *argument = nullptr;
        if (!std::strncmp(text, "peer ", 5)) {
            result.command = ConsoleCommand::Peer;
            argument = text + 5;
        }
        if (!std::strncmp(text, "ping ", 5)) {
            result.command = ConsoleCommand::Ping;
            argument = text + 5;
        }
        if (!std::strncmp(text, "logs ", 5)) {
            result.command = ConsoleCommand::Logs;
            argument = text + 5;
            base = 10;
        }
        if (argument) {
            char *end = nullptr;
            errno = 0;
            if (*argument == '-' || *argument == '+')
                return ConsoleRequest{0, generation_, result.request, ConsoleCommand::Invalid};
            result.argument = std::strtoull(argument, &end, base);
            if (errno || end == argument || *end || !result.argument)
                result.command = ConsoleCommand::Invalid;
        }
    }
    return result;
}
void ConsoleParser::feed(uint32_t generation, const uint8_t *bytes, size_t size, Sink sink, void *context) {
    if (generation != generation_) {
        generation_ = generation;
        used_ = 0;
        overflow_ = false;
    }
    for (size_t i = 0; i < size; ++i) {
        const auto c = bytes[i];
        if (c == '\r')
            continue;
        if (c == '\n') {
            if (used_ || overflow_)
                sink(context, parse());
            used_ = 0;
            overflow_ = false;
            continue;
        }
        if (c < 32 || c > 126 || used_ + 1 >= line_.size()) {
            overflow_ = true;
            continue;
        }
        if (!overflow_)
            line_[used_++] = char(c);
    }
}
DiagnosticsService::DiagnosticsService(comms::CommunicationPort &port, PeerService &peers,
                                       runtime::Dispatcher &dispatcher, diagnostics::Journal &journal,
                                       comms::Clock clock, MetricsSource metrics, EventOutput output,
                                       uint8_t ble_endpoint, bool mc)
    : port_(port), peers_(peers), dispatcher_(dispatcher), journal_(journal), clock_(clock),
      metrics_(metrics), output_(output), ble_endpoint_(ble_endpoint), mission_computer_(mc) {
    const bool a = inbox.subscribe(comms::incoming(icd::Type::Diagnostic), sizeof(comms::Inbound),
                                   runtime::Policy::Ordered);
    const bool b = inbox.subscribe(console_command_id, sizeof(ConsoleRequest), runtime::Policy::Ordered);
    const bool c = inbox.subscribe(probe_result_id, sizeof(ProbeResult), runtime::Policy::Ordered);
    assert(a && b && c);
    (void)a;
    (void)b;
    (void)c;
}
bool DiagnosticsService::line(uint32_t generation, const char *format, ...) {
    if (!mission_computer_)
        return false;
    ConsoleLine value{};
    value.generation = generation;
    va_list args;
    va_start(args, format);
    const auto length = std::vsnprintf(value.text, sizeof(value.text), format, args);
    va_end(args);
    if (length < 0)
        return false;
    value.size = uint8_t(std::min<size_t>(size_t(length), sizeof(value.text) - 1));
    const auto result = dispatcher_.deliver(runtime::destination(ble_endpoint_),
                                            runtime::Message::make(console_line_id, 0, value));
    if (!result.complete())
        ++drops_;
    return result.complete();
}
void DiagnosticsService::on_message(const runtime::Message &message, uint64_t) {
    if (message.id == console_command_id) {
        ConsoleRequest request{};
        if (!message.read(request))
            return;
        if (request.command == ConsoleCommand::WatchOn || request.command == ConsoleCommand::WatchOff) {
            dispatcher_.deliver(
                runtime::destination(ble_endpoint_),
                runtime::Message::make(
                    console_watch_id, request.request,
                    ConsoleWatch{request.generation, request.command == ConsoleCommand::WatchOn}));
            return;
        }
        if (reply_.command != ConsoleCommand::Invalid && reply_.generation == request.generation) {
            line(request.generation, "busy: wait for current reply");
            return;
        }
        if (request.command == ConsoleCommand::Ping) {
            const auto result = peers_.probe(request.argument, request.generation, request.request);
            line(request.generation,
                 result == runtime::Delivery::Accepted ? "probe %lu admitted" : "probe %lu rejected locally",
                 (unsigned long)request.request);
            return;
        }
        reply_ = request;
        cursor_ = 0;
        end_ = 0;
        page_ = 0;
        if (request.command == ConsoleCommand::Logs) {
            end_ = journal_.latest();
            const auto n = std::min<uint64_t>(request.argument, journal_.capacity());
            cursor_ = std::max(journal_.oldest(), end_ >= n ? end_ - n + 1 : uint64_t(1));
        }
        if (request.command == ConsoleCommand::Invalid)
            line(request.generation, "error: use help (commands end with LF)");
    } else if (message.id == probe_result_id) {
        ProbeResult result{};
        if (message.read(result))
            line(result.console_generation, "probe %lu %s %llu ms", (unsigned long)result.request,
                 name(result.outcome), (unsigned long long)result.elapsed_ms);
    } else if (message.id == comms::incoming(icd::Type::Diagnostic)) {
        comms::Inbound value{};
        if (!message.read(value))
            return;
        PeerRecord peer{};
        bool found = false;
        for (size_t i = 0; i < peers_.capacity(); ++i)
            if (peers_.peer(i, peer) && peer.route == value.route &&
                peer.status != PeerStatus::Disconnected) {
                found = true;
                break;
            }
        auto *session = port_.session(value.route);
        if (!found || !session) {
            ++drops_;
            return;
        }
        comms::Session::Guard guard(*session);
        if (!guard.active(value.route)) {
            ++drops_;
            return;
        }
        char text[48]{};
        const auto d = value.body.get<icd::Diagnostic>();
        std::snprintf(text, sizeof(text), "event=%u %.*s", unsigned(d.code),
                      int(std::min<size_t>(d.size, sizeof(d.text))), d.text);
        journal_.append(diagnostics::Event::RemoteEvent, clock_.now(), peer.descriptor.device,
                        peer.descriptor.boot, text, d.severity, d.source_ms);
    }
}
void DiagnosticsService::next_line() {
    if (reply_.command == ConsoleCommand::Invalid)
        return;
    const auto generation = reply_.generation;
    bool done = false;
    switch (reply_.command) {
    case ConsoleCommand::Help: {
        constexpr const char *help[] = {"help | status | peers | peer <hex-id>",
                                        "ping <hex-id> | stats | logs [count]", "watch on | watch off",
                                        "ACK=admission; peer ready=semantic handshake",
                                        "healthy keepalive is silent"};
        if (line(generation, "%s", help[cursor_]))
            ++cursor_;
        done = cursor_ == sizeof(help) / sizeof(help[0]);
        break;
    }
    case ConsoleCommand::Status: {
        const auto d = peers_.identity();
        if (cursor_ == 0) {
            if (line(generation, "MC %012llX boot=%016llX", (unsigned long long)d.device,
                     (unsigned long long)d.boot))
                ++cursor_;
        } else {
            const auto m = metrics_.read(metrics_.context);
            done = line(generation, "sessions=%lu/%u ready=%s", (unsigned long)m.sessions,
                        unsigned(port_.count()), d.readiness == icd::Readiness::Ready ? "yes" : "no");
        }
        break;
    }
    case ConsoleCommand::Peer: {
        if (page_ == 0) {
            bool found = false;
            for (size_t i = 0; i < peers_.capacity(); ++i) {
                if (peers_.peer(i, peer_reply_) && peer_reply_.descriptor.device == reply_.argument) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                done = line(generation, "unknown peer");
                break;
            }
        }
        const auto &p = peer_reply_;
        bool sent = false;
        switch (page_) {
        case 0:
            sent = line(generation, "%016llX %s", (unsigned long long)p.descriptor.device, name(p.status));
            break;
        case 1:
            sent = line(generation, "boot=%016llX protocol=%u role=%u", (unsigned long long)p.descriptor.boot,
                        unsigned(p.descriptor.protocol), unsigned(p.descriptor.role));
            break;
        case 2:
            sent = line(generation, "slot=%lu generation=%lu", (unsigned long)p.route.slot,
                        (unsigned long)p.route.generation);
            break;
        case 3:
            sent = line(generation, "reason=%u link=%s", unsigned(p.descriptor.reason), name(p.failure));
            break;
        default:
            sent = line(generation, "seen=%llu changed=%llu ms", (unsigned long long)p.last_seen,
                        (unsigned long long)p.changed_at);
            break;
        }
        if (sent)
            ++page_;
        done = page_ == 5;
        break;
    }
    case ConsoleCommand::Peers: {
        PeerRecord p{};
        bool found = false;
        while (cursor_ < peers_.capacity()) {
            if (peers_.peer(size_t(cursor_), p)) {
                found = true;
                break;
            }
            ++cursor_;
        }
        if (found) {
            if (line(generation, "%012llX %-12s g=%lu", (unsigned long long)p.descriptor.device,
                     name(p.status), (unsigned long)p.route.generation))
                ++cursor_;
        } else
            done = line(generation, "end peers");
        break;
    }
    case ConsoleCommand::Stats: {
        const auto m = metrics_.read(metrics_.context);
        if (cursor_ == 0) {
            if (line(generation, "heap=%lu min=%lu largest=%lu", (unsigned long)m.free_heap,
                     (unsigned long)m.minimum_heap, (unsigned long)m.largest_block))
                ++cursor_;
        } else
            done = line(generation, "stack-min=%lu stale=%lu drops=%lu", (unsigned long)m.minimum_stack,
                        (unsigned long)m.stale, (unsigned long)m.diagnostic_drops);
        break;
    }
    case ConsoleCommand::Logs: {
        if (cursor_ > end_) {
            done = line(generation, "end logs");
            break;
        }
        if (page_ == 0 && !journal_.read(cursor_, log_reply_)) {
            if (line(generation, "log gap before %llu", (unsigned long long)journal_.oldest()))
                cursor_ = journal_.oldest();
            break;
        }
        const auto &r = log_reply_;
        bool sent = false;
        switch (page_) {
        case 0:
            sent = line(generation, "#%llu at=%llu e=%u", (unsigned long long)r.sequence,
                        (unsigned long long)r.received_ms, unsigned(r.code));
            break;
        case 1:
            sent = line(generation, "id=%016llX boot=%016llX", (unsigned long long)r.device,
                        (unsigned long long)r.boot);
            break;
        case 2:
            sent = line(generation, "source=%llu repeat=%lu level=%u", (unsigned long long)r.source_ms,
                        (unsigned long)r.repeats, unsigned(r.severity));
            break;
        case 3:
            sent = line(generation, "  %.40s", r.text);
            break;
        default:
            sent = line(generation, "  %.40s", r.text + 40);
            break;
        }
        if (sent)
            ++page_;
        if (page_ == 5 || (page_ == 4 && r.size <= 40)) {
            ++cursor_;
            page_ = 0;
        }
        break;
    }
    default:
        done = true;
        break;
    }
    if (done)
        reply_.command = ConsoleCommand::Invalid;
}
void DiagnosticsService::on_tick(uint64_t) {
    const auto now = clock_.now();
    next_line();
    diagnostics::Record record{};
    if (output_cursor_ < journal_.oldest()) {
        drops_ += uint32_t(journal_.oldest() - output_cursor_);
        output_cursor_ = journal_.oldest();
    }
    if (journal_.read(output_cursor_, record)) {
        ++output_cursor_;
        if (output_.write)
            output_.write(output_.context, record);
    }
    if (mission_computer_ || now < next_network_)
        return;
    next_network_ = now + 250;
    PeerRecord peer{};
    bool connected = false;
    for (size_t i = 0; i < peers_.capacity(); ++i)
        if (peers_.peer(i, peer) && peer.status != PeerStatus::Disconnected) {
            connected = true;
            break;
        }
    if (!connected)
        return;
    network_cursor_ = std::max(network_cursor_, journal_.oldest());
    if (!journal_.read(network_cursor_, record))
        return;
    if (record.code == diagnostics::Event::RemoteEvent) {
        ++network_cursor_;
        return;
    }
    icd::Packet packet{};
    packet.type = icd::Type::Diagnostic;
    icd::Diagnostic d{};
    d.source_ms = record.received_ms;
    d.sequence = uint32_t(record.sequence);
    d.code = uint16_t(record.code);
    d.severity = record.severity;
    d.size = uint8_t(std::min<size_t>(record.size, sizeof(d.text)));
    std::memcpy(d.text, record.text, d.size);
    packet.body.set(d);
    if (port_.send(peer.route, packet) != comms::Post::Accepted)
        ++drops_;
    ++network_cursor_;
}
} // namespace raven::services
