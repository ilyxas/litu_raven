#include "raven_core/activity.hpp"
#include "raven_core/executor.hpp"
#include "raven_services/diagnostics_service.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

// Intentionally no peer_service.hpp: State and command consumers do not need
// the implementation that performs handshake/keepalive.
using namespace raven;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)

static_assert(std::is_base_of_v<runtime::Component, runtime::Activity>);
static_assert(std::is_base_of_v<runtime::Component, runtime::Service>);
static_assert(std::is_base_of_v<runtime::Service, services::DiagnosticsService>);
static_assert(sizeof(runtime::Activity) == sizeof(runtime::Component));
static_assert(sizeof(runtime::Service) == sizeof(runtime::Component));

namespace {
icd::DeviceDescriptor descriptor(icd::DeviceId id) {
    icd::DeviceDescriptor value{};
    value.device = id;
    value.boot = id + 100;
    value.role = icd::Role::Node;
    value.readiness = icd::Readiness::Ready;
    return value;
}

void state_snapshots() {
    comms::Session session;
    session.configure(0, 0);
    const auto route = session.open(1);
    services::PeerRecord records[2]{};
    services::PeerState state(descriptor(1), records, 2, &session, 1);
    const auto &reader = state;
    services::PeerRecord snapshot{};
    CHECK(reader.capacity() == 2 && reader.identity().device == 1);
    CHECK(!reader.peer(0, snapshot) && !reader.peer(2, snapshot));
    {
        services::PeerState::Write update(state);
        update.record(0) = {descriptor(2), route, 10, 10, services::PeerStatus::Ready, comms::Failure::None};
        update.note_stale();
    }
    CHECK(reader.stale_messages() == 1);
    CHECK(reader.peer(0, snapshot) && snapshot.status == services::PeerStatus::Ready);
    snapshot.descriptor.device = 99;
    CHECK(reader.peer(0, snapshot) && snapshot.descriptor.device == 2);

    // A reader must see closure without waiting for any Service tick.
    session.close(route, comms::Failure::TransportLost);
    CHECK(reader.peer(0, snapshot) && snapshot.status == services::PeerStatus::Disconnected);
    {
        comms::Session::Guard guard(session);
        guard.state.peer_retired = true;
    }
    CHECK(session.retire());
    const auto next = session.open(20);
    CHECK(next != route);
    // Reopening a slot does not revive a record from its previous generation.
    CHECK(reader.peer(0, snapshot) && snapshot.status == services::PeerStatus::Disconnected);
    CHECK(snapshot.route == route && snapshot.descriptor.device == 2);
    {
        services::PeerState::Write update(state);
        update.record(0) = {descriptor(3), next, 20, 20, services::PeerStatus::Ready, comms::Failure::None};
    }
    CHECK(reader.peer(0, snapshot) && snapshot.route == next && snapshot.descriptor.device == 3);
    CHECK(snapshot.status == services::PeerStatus::Ready);
}

void concurrent_state_reads() {
    comms::Session session;
    session.configure(0, 0);
    const auto route = session.open(1);
    services::PeerRecord records[1]{};
    services::PeerState state(descriptor(1), records, 1, &session, 1);
    {
        services::PeerState::Write update(state);
        update.record(0) = {descriptor(2), route, 2, 2, services::PeerStatus::Ready, comms::Failure::None};
    }
    std::atomic<bool> start{false};
    std::thread writer([&] {
        while (!start.load())
            std::this_thread::yield();
        for (unsigned i = 3; i < 10003; ++i) {
            services::PeerState::Write update(state);
            auto &record = update.record(0);
            record.descriptor = descriptor(i);
            record.last_seen = record.changed_at = i;
        }
    });
    start = true;
    for (unsigned i = 0; i < 10000; ++i) {
        services::PeerRecord snapshot{};
        CHECK(state.peer(0, snapshot));
        CHECK(snapshot.descriptor.boot == snapshot.descriptor.device + 100);
        CHECK(snapshot.last_seen == snapshot.descriptor.device);
        CHECK(snapshot.changed_at == snapshot.last_seen);
    }
    writer.join();
}

struct Console final : runtime::Component {
    std::vector<std::string> lines;
    Console() {
        CHECK(inbox.subscribe(services::console_line_id, sizeof(services::ConsoleLine),
                              runtime::Policy::Ordered));
    }
    void on_message(const runtime::Message &message, uint64_t) override {
        services::ConsoleLine value{};
        CHECK(message.read(value) && value.generation == 9);
        lines.emplace_back(value.text, value.size);
    }
    void on_tick(uint64_t) override {}
};

struct ProbeOwner final : runtime::Service {
    runtime::Dispatcher &dispatcher;
    uint8_t reply_endpoint;
    unsigned executions{};
    ProbeOwner(runtime::Dispatcher &routing, uint8_t reply) : dispatcher(routing), reply_endpoint(reply) {
        CHECK(inbox.subscribe(services::probe_request_id, sizeof(services::ProbeRequest),
                              runtime::Policy::Ordered));
    }
    void on_message(const runtime::Message &message, uint64_t) override {
        services::ProbeRequest request{};
        CHECK(message.read(request));
        CHECK(message.request == request.request && request.device == 2 && request.console_generation == 9);
        ++executions;
        const services::ProbeResult result{request.device, 12, request.console_generation, request.request,
                                           services::Outcome::Succeeded};
        CHECK(dispatcher
                  .deliver(runtime::destination(reply_endpoint),
                           runtime::Message::make(services::probe_result_id, request.request, result))
                  .complete());
    }
    void on_tick(uint64_t) override {}
};

void diagnostics_contracts() {
    constexpr uint8_t peer_endpoint = 5, diagnostic_endpoint = 7, console_endpoint = 9;
    uint64_t now = 100;
    comms::Clock clock{[](void *p) { return *static_cast<uint64_t *>(p); }, &now};
    comms::Session session;
    session.configure(0, 0);
    const auto route = session.open(now);
    services::PeerRecord peers[2]{};
    services::PeerState state(descriptor(1), peers, 2, &session, 1);
    {
        services::PeerState::Write update(state);
        update.record(0) = {descriptor(2),       route, now, now, services::PeerStatus::Ready,
                            comms::Failure::None};
    }
    runtime::Dispatcher dispatcher;
    comms::CommunicationPort port(dispatcher, &session, 1, peer_endpoint, diagnostic_endpoint);
    diagnostics::Record records[8]{};
    diagnostics::Journal journal(records, 8);
    services::DiagnosticsService diagnostics(port, state, dispatcher, journal, clock,
                                             {[](void *) { return services::Metrics{}; }, nullptr}, {},
                                             peer_endpoint, console_endpoint, true);
    ProbeOwner peer(dispatcher, diagnostic_endpoint);
    Console console;
    CHECK(dispatcher.bind(diagnostic_endpoint, diagnostics.inbox));
    CHECK(dispatcher.bind(console_endpoint, console.inbox));
    runtime::Executor diagnostic_owner(diagnostics, 50), peer_owner(peer, 50), console_owner(console, 50);
    auto command = [&](services::ConsoleCommand type, uint32_t id) {
        const services::ConsoleRequest request{2, 9, id, type};
        CHECK(dispatcher
                  .deliver(runtime::destination(diagnostic_endpoint),
                           runtime::Message::make(services::console_command_id, id, request))
                  .complete());
        diagnostic_owner.step(now);
        console_owner.step(now);
    };

    // No bound peer route: a public service method cannot bypass admission.
    command(services::ConsoleCommand::Ping, 1);
    CHECK(console.lines.back() == "probe 1 rejected locally" && peer.inbox.pending() == 0);
    CHECK(dispatcher.bind(peer_endpoint, peer.inbox));
    command(services::ConsoleCommand::Ping, 2);
    CHECK(console.lines.back() == "probe 2 admitted");
    CHECK(peer.inbox.pending() == 1 && peer.executions == 0);
    peer_owner.step(now);
    CHECK(peer.executions == 1); // Only the receiving owner's Executor performs the work.
    diagnostic_owner.step(now);
    console_owner.step(now);
    CHECK(console.lines.back() == "probe 2 success 12 ms");

    for (unsigned i = 0; i < runtime::Inbox::capacity; ++i)
        CHECK(dispatcher
                  .deliver(runtime::destination(peer_endpoint),
                           runtime::Message::make(services::probe_request_id, 3 + i,
                                                  services::ProbeRequest{2, 9, 3 + i}))
                  .complete());
    command(services::ConsoleCommand::Ping, 20);
    CHECK(console.lines.back() == "probe 20 rejected locally");
    CHECK(peer.inbox.pending() == runtime::Inbox::capacity && peer.executions == 1);

    // The real DiagnosticsService can answer from State without a PeerService instance.
    command(services::ConsoleCommand::Peers, 21);
    now += 50;
    diagnostic_owner.step(now);
    console_owner.step(now);
    CHECK(console.lines.back().find("000000000002 ready") != std::string::npos);
    now += 50;
    diagnostic_owner.step(now);
    console_owner.step(now);
    CHECK(console.lines.back() == "end peers");
}
} // namespace

int main() {
    state_snapshots();
    concurrent_state_reads();
    diagnostics_contracts();
    std::puts("PASS 3 service contract scenarios");
}
