#include "raven_comms/length_framing.hpp"
#include "raven_core/executor.hpp"
#include "raven_diagnostics/log_capture.hpp"
#include "raven_services/diagnostics_service.hpp"
#include "raven_services/peer_service.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
using namespace raven;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)

void codec() {
    icd::Packet input{};
    input.type = icd::Type::HelloRequest;
    input.requires_ack = true;
    input.exchange = 0x12345678;
    icd::Hello hello{};
    hello.device.device = 0x112233445566;
    hello.device.boot = 9;
    hello.uptime_ms = 0x0102030405060708;
    std::strcpy(hello.device.name, "node");
    input.body.set(hello);
    uint8_t bytes[icd::max_packet]{};
    CHECK(icd::encode(input, bytes, sizeof(bytes)) == 56);
    CHECK(bytes[0] == 1 && bytes[1] == 1 && bytes[2] == 0 && bytes[3] == 1 && bytes[4] == 0x12);
    CHECK(bytes[48] == 1 && bytes[55] == 8);
    icd::Packet output{};
    CHECK(icd::decode(bytes, 56, output) == icd::Decode::Ok);
    CHECK(output.exchange == input.exchange &&
          output.body.get<icd::Hello>().device.device == hello.device.device);
    CHECK(icd::decode(bytes, 55, output) == icd::Decode::Invalid);
    bytes[0] = 99;
    CHECK(icd::decode(bytes, 56, output) == icd::Decode::UnsupportedVersion);
    input.requires_ack = false;
    input.type = icd::Type::KeepAliveResponse;
    input.body.set(icd::Heartbeat{0x1122334455667788, 9, icd::Reason::None, icd::Readiness::Ready, 0});
    CHECK(icd::encode(input, bytes, sizeof(bytes)) == 28);
    CHECK(bytes[8] == 0x11 && bytes[15] == 0x88 && bytes[26] == 1 && bytes[27] == 0);
    CHECK(icd::decode(bytes, 28, output) == icd::Decode::Ok);
    CHECK(output.body.get<icd::Heartbeat>().origin_ms == 0x1122334455667788);
    input.type = icd::Type::Receipt;
    input.body.set(icd::Receipt{icd::Type::HelloRequest, icd::Admission::Full, 0});
    CHECK(icd::encode(input, bytes, sizeof(bytes)) == 12 && bytes[9] == 1 && bytes[10] == 1);
    CHECK(icd::decode(bytes, 12, output) == icd::Decode::Ok);
    CHECK(output.body.get<icd::Receipt>().admission == icd::Admission::Full);
    input.type = icd::Type::Diagnostic;
    icd::Diagnostic diagnostic{55, 42, 11, 3, 4, "test"};
    input.body.set(diagnostic);
    CHECK(icd::encode(input, bytes, sizeof(bytes)) == 28 && bytes[23] == 4);
    CHECK(icd::decode(bytes, 28, output) == icd::Decode::Ok);
    CHECK(output.body.get<icd::Diagnostic>().source_ms == 55 &&
          !std::memcmp(output.body.get<icd::Diagnostic>().text, "test", 4));
}
struct TestComponent : runtime::Component {
    unsigned ticks{}, normal{}, critical{}, latest{};
    TestComponent() {
        CHECK(inbox.subscribe(1, sizeof(uint32_t), runtime::Policy::Ordered));
        CHECK(inbox.subscribe(2, sizeof(uint32_t), runtime::Policy::Critical));
        CHECK(inbox.subscribe(3, sizeof(uint32_t), runtime::Policy::Latest));
    }
    void on_message(const runtime::Message &m, uint64_t) override {
        if (m.id == 1)
            ++normal;
        if (m.id == 2) {
            ++critical;
            if (critical < 100)
                inbox.post(runtime::Message::make(2, 0, uint32_t(1)));
        }
        if (m.id == 3)
            ++latest;
    }
    void on_tick(uint64_t) override { ++ticks; }
};
void core() {
    TestComponent c;
    runtime::Executor e(c, 10);
    runtime::Dispatcher d;
    CHECK(d.bind(12, c.inbox));
    for (unsigned i = 0; i < runtime::Inbox::capacity; ++i)
        CHECK(d.deliver(runtime::destination(12), runtime::Message::make(1, i, i)).complete());
    CHECK(d.deliver(runtime::destination(12), runtime::Message::make(1, 99, uint32_t(0))).reason ==
          runtime::Delivery::Full);
    for (unsigned i = 0; i < 1000; ++i)
        CHECK(c.inbox.post(runtime::Message::make(2, i, i)) == runtime::Delivery::Accepted);
    CHECK(c.inbox.pending() == 9);
    for (uint64_t now = 0; now < 200; now += 10)
        e.step(now);
    CHECK(c.ticks == 20 && c.critical == 100);
    while (c.inbox.pending())
        e.step(200);
    CHECK(c.normal == 8);
    for (unsigned i = 0; i < 100; ++i)
        CHECK(c.inbox.post(runtime::Message::make(3, i, i)) == runtime::Delivery::Accepted);
    CHECK(c.inbox.pending() == 1);
    e.step(210);
    CHECK(c.latest == 1);
}
void framing() {
    icd::Packet p{};
    p.type = icd::Type::KeepAliveRequest;
    p.exchange = 1;
    uint8_t bytes[2 * (icd::max_packet + 2)]{};
    uint8_t encoded[icd::max_packet]{};
    const auto encoded_size = icd::encode(p, encoded, sizeof(encoded));
    const auto n = comms::length_frame(encoded, encoded_size, bytes, sizeof(bytes));
    CHECK(n == 30);
    comms::LengthFrameReader reader;
    unsigned received = 0;
    auto sink = [](void *ctx, const uint8_t *data, size_t size, uint64_t) {
        icd::Packet packet{};
        CHECK(icd::decode(data, size, packet) == icd::Decode::Ok);
        ++*static_cast<unsigned *>(ctx);
        return true;
    };
    for (size_t i = 0; i < n; ++i)
        CHECK(reader.feed(bytes + i, 1, i, sink, &received));
    CHECK(received == 1);
    std::memcpy(bytes + n, bytes, n);
    CHECK(reader.feed(bytes, 2 * n, 100, sink, &received));
    CHECK(received == 3);
    CHECK(reader.feed(bytes, 1, 200, sink, &received));
    CHECK(reader.expired(1200));
    CHECK(!reader.feed(bytes + 1, n - 1, 1200, sink, &received));
    reader.reset();
    uint8_t invalid[] = {0xff, 0xff};
    CHECK(!reader.feed(invalid, 2, 2000, sink, &received));
    CHECK(!reader.feed(bytes, n, 2001, sink, &received));
    reader.reset();
    CHECK(reader.feed(bytes, n, 2002, sink, &received));
}
struct Device;
struct Link : comms::PacketWriter {
    Device *remote{};
    comms::SessionKey remote_key{};
    bool drop{}, fail{};
    unsigned sent{};
    std::function<void()> before_return;
    std::function<bool(const uint8_t *, size_t)> deliver;
    comms::Failure write_packet(comms::SessionKey, const uint8_t *, size_t, uint64_t) override;
};
icd::DeviceDescriptor identity(icd::Role role, uint64_t id) {
    icd::DeviceDescriptor d{};
    d.device = id;
    d.boot = id + 100;
    d.role = role;
    d.readiness = icd::Readiness::Ready;
    d.reason = icd::Reason::None;
    return d;
}
struct Sink : runtime::Component {
    services::ProbeResult last{};
    unsigned results{};
    Sink() {
        CHECK(inbox.subscribe(comms::incoming(icd::Type::Diagnostic), sizeof(comms::Inbound),
                              runtime::Policy::Ordered));
        CHECK(inbox.subscribe(services::probe_result_id, sizeof(last), runtime::Policy::Ordered));
    }
    void on_message(const runtime::Message &m, uint64_t) override {
        if (m.id == services::probe_result_id) {
            CHECK(m.read(last));
            ++results;
        }
    }
    void on_tick(uint64_t) override {}
};
struct Device {
    uint64_t now{100};
    comms::Clock clock{[](void *p) { return static_cast<Device *>(p)->now; }, this};
    comms::Session session;
    runtime::Dispatcher dispatcher;
    comms::CommunicationPort port{dispatcher, &session, 1, 1, 2};
    diagnostics::Record records[16]{};
    diagnostics::Journal journal{records, 16};
    services::PeerSlot slot;
    services::PeerRecord peers[2]{};
    services::PeerService service;
    Link link;
    comms::TxComponent tx{session, link, clock};
    Sink sink;
    runtime::Executor service_executor{service, 50}, tx_executor{tx, 50}, sink_executor{sink, 50};
    comms::LengthFrameReader reader;
    comms::SessionKey key;
    Device(icd::Role role, uint64_t id)
        : service(port, dispatcher, identity(role, id), clock, &slot, peers, 2, journal, 2) {
        session.configure(0, 0);
        CHECK(dispatcher.bind(0, tx.inbox));
        CHECK(dispatcher.bind(1, service.inbox));
        CHECK(dispatcher.bind(2, sink.inbox));
        key = session.open(now);
        CHECK(bool(key));
    }
    void step() {
        service_executor.step(now);
        tx_executor.step(now);
        sink_executor.step(now);
    }
    bool receive(const uint8_t *bytes, size_t size) {
        return reader.feed(
            bytes, size, now,
            [](void *ctx, const uint8_t *data, size_t length, uint64_t received) {
                auto &d = *static_cast<Device *>(ctx);
                icd::Packet packet{};
                if (icd::decode(data, length, packet) != icd::Decode::Ok)
                    return false;
                d.port.receive(d.key, packet, received);
                return d.session.active(d.key);
            },
            this);
    }
};
comms::Failure Link::write_packet(comms::SessionKey, const uint8_t *packet, size_t packet_size, uint64_t) {
    uint8_t bytes[icd::max_packet + 2]{};
    const auto size = comms::length_frame(packet, packet_size, bytes, sizeof(bytes));
    CHECK(size);
    ++sent;
    if (fail)
        return comms::Failure::WriteError;
    if (!drop && deliver && !deliver(bytes, size))
        return comms::Failure::TransportLost;
    if (!drop && remote && remote->key == remote_key) {
        // Exercise real framing on fragmented writes, with independently driven owners.
        for (size_t i = 0; i < size; ++i)
            if (!remote->receive(bytes + i, 1))
                return comms::Failure::TransportLost;
    }
    if (before_return)
        before_return();
    return comms::Failure::None;
}
struct Pair {
    Device mc{icd::Role::MissionComputer, 1}, node{icd::Role::Node, 2};
    Pair() {
        mc.link.remote = &node;
        mc.link.remote_key = node.key;
        node.link.remote = &mc;
        node.link.remote_key = mc.key;
    }
    void run(unsigned steps) {
        while (steps--) {
            mc.step();
            node.step();
            mc.now += 10;
            node.now += 10;
        }
    }
    void ready() {
        run(20);
        services::PeerRecord p{};
        CHECK(mc.service.peer(0, p) && p.status == services::PeerStatus::Ready);
        CHECK(node.service.peer(0, p) && p.status == services::PeerStatus::Ready);
    }
};
void exchange() {
    Pair p;
    p.ready();
    p.run(600);
    CHECK(p.mc.session.active(p.mc.key) && p.node.session.active(p.node.key));
    CHECK(p.mc.link.sent > 5 && p.node.link.sent > 5);
    CHECK(p.mc.service.probe(2, 4, 5) == runtime::Delivery::Accepted);
    p.run(20);
    CHECK(p.mc.sink.results == 1 && p.mc.sink.last.outcome == services::Outcome::Succeeded);
    CHECK(p.mc.sink.last.console_generation == 4 && p.mc.sink.last.request == 5);
}
void timing() {
    Device mc(icd::Role::MissionComputer, 1);
    mc.link.drop = true;
    mc.service.on_tick(mc.now); // Locally admitted at 100, no TX execution yet.
    CHECK(mc.slot.exchanges[0].active && !mc.slot.exchanges[0].has_written);
    mc.now = 349;
    mc.service.on_tick(mc.now);
    CHECK(mc.session.active(mc.key));
    mc.now = 350;
    mc.service.on_tick(mc.now);
    CHECK(mc.session.snapshot().failure == comms::Failure::SendDeadline);
    Device sent(icd::Role::MissionComputer, 3);
    sent.link.drop = true;
    sent.service.on_tick(sent.now);
    sent.now = 300;
    sent.tx_executor.step(sent.now);
    sent.service.on_tick(sent.now);
    CHECK(sent.slot.exchanges[0].deadline == 3300);
    icd::Packet ack{};
    ack.type = icd::Type::Receipt;
    ack.exchange = sent.slot.exchanges[0].request.exchange;
    ack.body.set(icd::Receipt{icd::Type::HelloRequest, icd::Admission::Accepted, 0});
    sent.now = 2000;
    CHECK(sent.port.receive(sent.key, ack, sent.now) == comms::Post::Accepted);
    sent.service_executor.step(sent.now);
    CHECK(sent.slot.exchanges[0].deadline == 3300);
    // Drain the scheduled retry; no deadline extension even after another Written.
    sent.tx_executor.step(sent.now);
    sent.service.on_tick(sent.now);
    CHECK(sent.slot.exchanges[0].deadline == 3300);
    sent.now = 3300;
    sent.service.on_tick(sent.now);
    CHECK(sent.session.snapshot().failure == comms::Failure::ResponseTimeout);
}
void response_before_written() {
    Pair p;
    p.mc.link.before_return = [&] {
        // Remote owner executes and sends both the semantic response and receipt
        // while the local writer has not published Written yet.
        p.node.service_executor.step(p.node.now);
        p.node.tx_executor.step(p.node.now);
        p.node.tx_executor.step(p.node.now);
        p.mc.service_executor.step(p.mc.now);
        p.mc.service_executor.step(p.mc.now);
    };
    p.mc.service_executor.step(p.mc.now);
    p.mc.tx_executor.step(p.mc.now);
    CHECK(!p.mc.slot.exchanges[0].active && p.mc.slot.exchanges[0].outcome == services::Outcome::Succeeded);
    p.mc.link.before_return = {};
    p.run(20);
    CHECK(p.mc.session.active(p.mc.key));
}
void stale_generation() {
    Pair p;
    p.ready();
    const auto old = p.mc.key;
    icd::Packet late{};
    late.type = icd::Type::HelloRequest;
    late.exchange = 999;
    late.body.set(icd::Hello{identity(icd::Role::Node, 77), p.mc.now});
    CHECK(p.mc.port.receive(old, late, p.mc.now) == comms::Post::Accepted);
    p.mc.session.close(old, comms::Failure::TransportLost);
    p.mc.service.on_tick(p.mc.now);
    while (p.mc.tx.inbox.pending())
        p.mc.tx_executor.step(p.mc.now);
    CHECK(p.mc.session.retire());
    p.mc.key = p.mc.session.open(p.mc.now);
    CHECK(p.mc.key != old);
    p.mc.service.on_tick(p.mc.now);
    p.mc.service_executor.step(p.mc.now);
    CHECK(p.mc.service.stale_messages() == 1);
    services::PeerRecord peer{};
    CHECK(p.mc.service.peer(0, peer));
    CHECK(peer.descriptor.device == 2 && peer.status == services::PeerStatus::Disconnected);
    CHECK(!p.mc.service.peer(1, peer));
    CHECK(p.mc.port.send(old, late) == comms::Post::StaleRoute);
    p.mc.session.close(old, comms::Failure::Protocol);
    CHECK(p.mc.session.active(p.mc.key));
}
void close_commit_serialization() {
    comms::Session session;
    session.configure(0, 0);
    const auto key = session.open(0);
    std::atomic<bool> closing{false}, closed{false};
    std::thread closer;
    {
        comms::Session::Guard guard(session);
        CHECK(guard.active(key));
        closer = std::thread([&] {
            closing = true;
            session.close(key, comms::Failure::TransportLost);
            closed = true;
        });
        while (!closing.load())
            std::this_thread::yield();
        CHECK(!closed.load()); // close cannot invalidate between the check and commit
        guard.state.opened_at = 123;
    }
    closer.join();
    CHECK(closed.load() && !session.active(key));
}
void lost_keepalive_and_overload() {
    Pair p;
    p.ready();
    p.node.link.drop = true;
    p.run(500);
    CHECK(!p.mc.session.active(p.mc.key));
    Device node(icd::Role::Node, 8);
    icd::Packet out{};
    out.type = icd::Type::KeepAliveResponse;
    out.exchange = 1;
    for (unsigned i = 0; i < runtime::Inbox::capacity; ++i)
        CHECK(node.port.send(node.key, out) == comms::Post::Accepted);
    icd::Packet hello{};
    hello.type = icd::Type::HelloRequest;
    hello.exchange = 2;
    hello.requires_ack = true;
    hello.body.set(icd::Hello{identity(icd::Role::MissionComputer, 1), 0});
    CHECK(node.port.receive(node.key, hello, 0) == comms::Post::Accepted);
    CHECK(node.service.inbox.pending() == 1 && node.session.snapshot().failure == comms::Failure::LocalFull);
}
void journal() {
    diagnostics::Record records[2];
    diagnostics::Journal journal(records, 2);
    auto a = journal.append(diagnostics::Event::Boot, 1, 1, 1, "boot");
    CHECK(journal.append(diagnostics::Event::Boot, 2, 1, 1, "boot") == a);
    diagnostics::Record r{};
    CHECK(journal.read(a, r) && r.repeats == 2);
    journal.append(diagnostics::Event::PeerReady, 3, 2, 1, "ready");
    journal.append(diagnostics::Event::LinkLost, 4, 2, 1, "lost");
    CHECK(!journal.read(a, r) && journal.oldest() == 2);
}
void console_parser() {
    services::ConsoleParser parser;
    std::vector<services::ConsoleRequest> commands;
    auto sink = [](void *p, const services::ConsoleRequest &c) {
        static_cast<std::vector<services::ConsoleRequest> *>(p)->push_back(c);
    };
    auto feed = [&](uint32_t generation, const char *text) {
        parser.feed(generation, reinterpret_cast<const uint8_t *>(text), std::strlen(text), sink, &commands);
    };
    feed(1, "C:pi");
    feed(1, "ng 0123abcd\r\nstatus\nwatch off\n");
    CHECK(commands.size() == 3 && commands[0].argument == 0x123abcd &&
          commands[0].command == services::ConsoleCommand::Ping);
    CHECK(commands[1].command == services::ConsoleCommand::Status &&
          commands[2].command == services::ConsoleCommand::WatchOff);
    feed(1, "pi");
    feed(2, "help\n");
    CHECK(commands.back().command == services::ConsoleCommand::Help && commands.back().generation == 2);
    feed(2, "ping -1\n");
    CHECK(commands.back().command == services::ConsoleCommand::Invalid);
    const std::string too_long(200, 'x');
    feed(2, too_long.c_str());
    feed(2, "help\n");
    CHECK(commands.back().command == services::ConsoleCommand::Invalid);
    feed(2, "logs 2\n");
    CHECK(commands.back().command == services::ConsoleCommand::Logs && commands.back().argument == 2);
}
void tickets_and_reuse() {
    comms::Session s;
    s.configure(0, 0);
    comms::SessionKey previous{};
    comms::SendToken old_ticket{};
    for (unsigned i = 0; i < 200; ++i) {
        const auto key = s.open(i * 1000);
        CHECK(key.generation == i + 1 && key != previous);
        auto a = s.reserve(key, 1, icd::Type::HelloRequest, i * 1000);
        auto b = s.reserve(key, 2, icd::Type::KeepAliveRequest, i * 1000);
        CHECK(a && b && !s.reserve(key, 3, icd::Type::HelloRequest, 0));
        uint64_t deadline{};
        CHECK(s.begin(a, key, i * 1000 + 200, deadline));
        CHECK(deadline == i * 1000 + 250);
        if (old_ticket)
            s.finish(old_ticket, previous, i * 1000, comms::Failure::None);
        CHECK(s.progress(a).phase == comms::SendPhase::Writing);
        CHECK(s.acquire_io(key));
        s.close(key, comms::Failure::WriteError);
        CHECK(!s.active(key) && !s.retire());
        {
            comms::Session::Guard g(s);
            g.state.peer_retired = true;
        }
        CHECK(!s.retire());
        s.release_io();
        CHECK(s.retire());
        old_ticket = a;
        previous = key;
    }
}
void six_peer_profile() {
    constexpr size_t count = 6;
    uint64_t now = 100;
    comms::Clock clock{[](void *p) { return *static_cast<uint64_t *>(p); }, &now};
    std::array<comms::Session, count> sessions;
    runtime::Dispatcher dispatcher;
    comms::CommunicationPort port{dispatcher, sessions.data(), count, count, count + 1};
    diagnostics::Record records[64]{};
    diagnostics::Journal journal(records, 64);
    services::PeerSlot slots[count]{};
    services::PeerRecord peers[count * 2]{};
    services::PeerService mc(port, dispatcher, identity(icd::Role::MissionComputer, 1), clock, slots, peers,
                             count * 2, journal, count + 1);
    runtime::Executor owner(mc, 50);
    Sink sink;
    CHECK(dispatcher.bind(count, mc.inbox));
    CHECK(dispatcher.bind(count + 1, sink.inbox));
    std::array<std::unique_ptr<Device>, count> nodes;
    std::array<Link, count> links;
    std::array<std::unique_ptr<comms::TxComponent>, count> tx;
    std::array<std::unique_ptr<runtime::Executor>, count> writers;
    std::array<comms::LengthFrameReader, count> readers;
    std::array<comms::SessionKey, count> keys;
    for (size_t i = 0; i < count; ++i) {
        sessions[i].configure(uint32_t(i), uint8_t(i));
        keys[i] = sessions[i].open(now);
        nodes[i] = std::make_unique<Device>(icd::Role::Node, 10 + i);
        links[i].remote = nodes[i].get();
        links[i].remote_key = nodes[i]->key;
        tx[i] = std::make_unique<comms::TxComponent>(sessions[i], links[i], clock);
        writers[i] = std::make_unique<runtime::Executor>(*tx[i], 50);
        CHECK(dispatcher.bind(uint8_t(i), tx[i]->inbox));
        nodes[i]->link.deliver = [&, i](const uint8_t *bytes, size_t size) {
            struct Context {
                comms::CommunicationPort *port;
                comms::SessionKey key;
            } context{&port, keys[i]};
            return readers[i].feed(
                bytes, size, now,
                [](void *p, const uint8_t *data, size_t n, uint64_t received) {
                    auto &c = *static_cast<Context *>(p);
                    icd::Packet packet{};
                    CHECK(icd::decode(data, n, packet) == icd::Decode::Ok);
                    c.port->receive(c.key, packet, received);
                    return c.port->session(c.key)->active(c.key);
                },
                &context);
        };
    }
    auto advance = [&](unsigned steps) {
        while (steps--) {
            // Model OwnerTask's bounded drain, not a one-message-per-tick loop.
            for (unsigned batch = 0; batch < 8; ++batch)
                owner.step(now);
            for (size_t i = 0; i < count; ++i) {
                writers[i]->step(now);
                nodes[i]->now = now;
                nodes[i]->step();
            }
            now += 10;
        }
    };
    advance(500);
    for (size_t i = 0; i < count; ++i) {
        services::PeerRecord p{};
        CHECK(mc.peer(i, p) && p.status == services::PeerStatus::Ready);
    }
    // Loss and late data on one route must leave all other peer bindings intact.
    sessions[2].close(keys[2], comms::Failure::TransportLost);
    owner.step(now);
    advance(100);
    for (size_t i = 0; i < count; ++i)
        if (i != 2)
            CHECK(sessions[i].active(keys[i]));
    // The same DeviceId on another live route is rejected, never a takeover.
    icd::Packet duplicate{};
    duplicate.type = icd::Type::HelloRequest;
    duplicate.exchange = 999;
    duplicate.body.set(icd::Hello{identity(icd::Role::Node, 10), 0});
    CHECK(port.receive(keys[4], duplicate, now) == comms::Post::Accepted);
    owner.step(now);
    CHECK(peers[0].descriptor.device == 10 && peers[0].route == keys[0]);
    CHECK(sessions[0].active(keys[0]));
    // Also reject a duplicate on an unestablished, reused slot, not only an
    // identity mutation on an already bound session.
    {
        comms::Session::Guard guard(sessions[2]);
        guard.state.peer_retired = true;
    }
    while (tx[2]->inbox.pending())
        writers[2]->step(now);
    CHECK(sessions[2].retire());
    keys[2] = sessions[2].open(now);
    mc.on_tick(now);
    CHECK(!slots[2].established);
    CHECK(port.receive(keys[2], duplicate, now) == comms::Post::Accepted);
    for (unsigned n = 0; n < 8; ++n)
        owner.step(now);
    CHECK(!slots[2].established && peers[0].route == keys[0] && sessions[0].active(keys[0]));
}
void diagnostic_bound() {
    Device d(icd::Role::Node, 4);
    icd::Packet event{};
    event.type = icd::Type::Diagnostic;
    CHECK(d.port.send(d.key, event) == comms::Post::Accepted);
    CHECK(d.port.send(d.key, event) == comms::Post::Full);
    icd::Packet ack{};
    ack.type = icd::Type::Receipt;
    ack.exchange = 8;
    ack.body.set(icd::Receipt{icd::Type::HelloRequest, icd::Admission::Accepted, 0});
    CHECK(d.port.send(d.key, ack) == comms::Post::Accepted);
    d.tx_executor.step(d.now);
    CHECK(d.port.send(d.key, event) == comms::Post::Accepted);
    CHECK(d.tx.inbox.pending() == 2); // optional traffic leaves room for receipts
}
void captured_logs() {
    diagnostics::Record records[8]{};
    diagnostics::Journal journal(records, 8);
    diagnostics::LogCapture capture;
    capture.feed("W (123) wifi: ", 1, journal, 1, 2);
    capture.feed("radio warning", 1, journal, 1, 2);
    CHECK(journal.latest() == 0);
    capture.feed("\n", 1, journal, 1, 2);
    capture.feed("W (456) wifi: radio warning\n", 2, journal, 1, 2);
    diagnostics::Record r{};
    CHECK(journal.read(1, r) && r.repeats == 2 && !std::strcmp(r.text, "wifi: radio warning"));
    capture.feed("I (900) wifi: normal\n", 3, journal, 1, 2);
    CHECK(journal.latest() == 1);
    capture.feed("E (999) socket: failure\n", 4, journal, 1, 2);
    CHECK(journal.read(2, r) && r.severity == 3 && !std::strcmp(r.text, "socket: failure"));
    capture.feed("W (1000) incomplete", 5, journal, 1, 2);
    capture.reset();
    capture.feed("I (1001) fresh\n", 6, journal, 1, 2);
    CHECK(journal.latest() == 2);
}
void packet_boundary() {
    struct Writer final : comms::PacketWriter {
        icd::Packet received{};
        unsigned count{};
        comms::Failure write_packet(comms::SessionKey, const uint8_t *bytes, size_t size, uint64_t) override {
            CHECK(icd::decode(bytes, size, received) == icd::Decode::Ok);
            ++count;
            return comms::Failure::None;
        }
    } writer;
    uint64_t now = 1;
    comms::Session session;
    session.configure(0, 0);
    auto route = session.open(now);
    comms::TxComponent tx(session, writer, {[](void *p) { return *static_cast<uint64_t *>(p); }, &now});
    runtime::Dispatcher dispatcher;
    CHECK(dispatcher.bind(0, tx.inbox));
    comms::CommunicationPort port(dispatcher, &session, 1, 1, 2);
    icd::Packet packet{};
    packet.type = icd::Type::KeepAliveResponse;
    packet.exchange = 42;
    CHECK(port.send(route, packet) == comms::Post::Accepted);
    runtime::Executor owner(tx, 50);
    owner.step(now);
    CHECK(writer.count == 1 && writer.received.exchange == 42);
}
void console_journal_reply() {
    Device d(icd::Role::Node, 5);
    struct Console final : runtime::Component {
        std::vector<std::string> lines;
        Console() {
            CHECK(inbox.subscribe(services::console_line_id, sizeof(services::ConsoleLine),
                                  runtime::Policy::Ordered));
        }
        void on_tick(uint64_t) override {}
        void on_message(const runtime::Message &message, uint64_t) override {
            services::ConsoleLine line{};
            CHECK(message.read(line));
            CHECK(line.generation == 9);
            lines.emplace_back(line.text, line.size);
        }
    } console;
    CHECK(d.dispatcher.bind(3, console.inbox));
    services::DiagnosticsService service(d.port, d.service, d.dispatcher, d.journal, d.clock,
                                         {[](void *) { return services::Metrics{}; }, nullptr}, {}, 3, true);
    runtime::Executor output(console, 50);
    const char *long_text = "0123456789012345678901234567890123456789abcdefghijABCDEFGHIJ-tail-preserved";
    d.journal.append(diagnostics::Event::SystemWarning, 100, 1, 2, long_text);
    services::ConsoleRequest request{1, 9, 1, services::ConsoleCommand::Logs};
    service.on_message(runtime::Message::make(services::console_command_id, 1, request), d.now);
    for (unsigned i = 0; i < 10; ++i) {
        service.on_tick(d.now);
        output.step(d.now);
        d.now += 50;
    }
    CHECK(console.lines.size() == 6 && console.lines.back() == "end logs");
    CHECK(console.lines[3].substr(2) + console.lines[4].substr(2) == long_text);
}
int main() {
    codec();
    core();
    framing();
    exchange();
    timing();
    response_before_written();
    stale_generation();
    close_commit_serialization();
    lost_keepalive_and_overload();
    journal();
    console_parser();
    tickets_and_reuse();
    six_peer_profile();
    diagnostic_bound();
    captured_logs();
    packet_boundary();
    console_journal_reply();
    std::puts("PASS 17 communication scenarios");
}
