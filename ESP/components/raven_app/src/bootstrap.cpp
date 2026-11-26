#include "raven_app/bootstrap.hpp"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "raven_platform/clock.hpp"
#include "raven_platform/system_log.hpp"
#include "raven_platform/tcp_network.hpp"
#include "raven_services/diagnostics_service.hpp"
#include "raven_services/peer_service.hpp"
#include "sdkconfig.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <new>
#include <type_traits>
#ifdef CONFIG_RAVEN_BLE_ENABLED
#include "raven_platform/ble_console.hpp"
#endif

namespace raven {
#ifdef CONFIG_RAVEN_TCP_BOARD_PROBE
void run_tcp_board_probe(WifiNetwork &wifi, uint16_t port);
#endif
namespace {
#ifdef CONFIG_RAVEN_MISSION_COMPUTER
constexpr bool mission_computer = true;
constexpr size_t session_count = CONFIG_RAVEN_MAX_SESSIONS, journal_capacity = 64;
#else
constexpr bool mission_computer = false;
constexpr size_t session_count = 1, journal_capacity = 32;
#endif
constexpr uint8_t peer_endpoint = session_count, diagnostics_endpoint = session_count + 1,
                  ble_endpoint = session_count + 2;
static_assert(ble_endpoint < 16, "Profile exceeds local Dispatcher address space");
static_assert(session_count + (mission_computer ? 1 : 0) <= CONFIG_LWIP_MAX_SOCKETS,
              "Increase the socket resource profile together with the session count");
icd::DeviceDescriptor identity() {
    icd::DeviceDescriptor d{};
    uint8_t mac[6]{};
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    for (auto byte : mac)
        d.device = (d.device << 8) | byte;
    esp_fill_random(&d.boot, sizeof(d.boot));
    if (!d.boot)
        d.boot = 1;
    d.role = mission_computer ? icd::Role::MissionComputer : icd::Role::Node;
    d.readiness = icd::Readiness::Ready;
    d.reason = icd::Reason::None;
    d.capabilities = 1;
    std::snprintf(d.name, sizeof(d.name), "%s-%04X", mission_computer ? "mc" : "node",
                  unsigned(d.device & 0xffff));
    return d;
}
struct Runtime;
services::Metrics metrics(void *context);
void fail_console(void *context, uint32_t generation);
void serial_event(void *, const diagnostics::Record &r) {
    ESP_LOGI("Raven", "event=%u peer=%012llX boot=%016llX time=%llu %s (x%lu)", unsigned(r.code),
             (unsigned long long)r.device, (unsigned long long)r.boot, (unsigned long long)r.received_ms,
             r.text, (unsigned long)r.repeats);
}
// Composition only: reference lifetimes, capacities and endpoint assignment.
// Channel/peer logic is shared by both projects and contains no board list.
struct Runtime {
    icd::DeviceDescriptor local{identity()};
    comms::Clock clock{[](void *) { return now_ms(); }, nullptr};
    std::array<comms::Session, session_count> sessions;
    runtime::Dispatcher dispatcher;
    comms::CommunicationPort port{dispatcher, sessions.data(), session_count, peer_endpoint,
                                  diagnostics_endpoint};
    std::array<diagnostics::Record, journal_capacity> records{};
    diagnostics::Journal journal{records.data(), records.size()};
    std::array<services::PeerSlot, session_count> slots{};
    std::array<services::PeerRecord, session_count * 2> peer_records{};
    services::PeerState peer_state{local, peer_records.data(), peer_records.size(), sessions.data(),
                                   sessions.size()};
    services::PeerService peers{port,         dispatcher, peer_state,           clock,
                                slots.data(), journal,    diagnostics_endpoint, {fail_console, this}};
    services::DiagnosticsService diagnostics{port,
                                             peer_state,
                                             dispatcher,
                                             journal,
                                             clock,
                                             {metrics, this},
                                             {serial_event, nullptr},
                                             peer_endpoint,
                                             ble_endpoint,
                                             mission_computer,
                                             {fail_console, this}};
    OwnerTask peer_owner{peers, "peers", 5, 50, 4096};
    OwnerTask diagnostic_owner{diagnostics, "diagnostics", 3, 50, 4096};
    struct alignas(TcpEndpoint) EndpointStorage {
        std::array<std::byte, sizeof(TcpEndpoint)> bytes;
    };
    std::array<EndpointStorage, session_count> endpoint_storage{};
    std::array<TcpEndpoint *, session_count> endpoints{};
    TcpManager manager;
#ifdef CONFIG_RAVEN_BLE_ENABLED
    BleSerialServer ble;
    BleConsoleTx ble_tx{ble, journal};
    OwnerTask ble_owner{ble_tx, "ble_tx", 3};
    BleConsoleRx ble_rx{ble, dispatcher, diagnostics_endpoint};
#endif
    explicit Runtime(WifiNetwork &wifi)
        : manager(wifi, endpoints.data(), endpoints.size(), mission_computer, CONFIG_RAVEN_TCP_PORT) {}
};
void fail_console(void *context, uint32_t generation) {
#ifdef CONFIG_RAVEN_BLE_ENABLED
    static_cast<Runtime *>(context)->ble.disconnect(generation);
#else
    // This composition exposes no console connection or manual-probe ingress.
    (void)context;
    (void)generation;
#endif
}
services::Metrics metrics(void *context) {
    auto &r = *static_cast<Runtime *>(context);
    constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT | MALLOC_CAP_DMA;
    services::Metrics m{};
    m.free_heap = heap_caps_get_free_size(caps);
    m.minimum_heap = heap_caps_get_minimum_free_size(caps);
    m.largest_block = heap_caps_get_largest_free_block(caps);
    m.minimum_stack =
        std::min({r.peer_owner.stack_free(), r.diagnostic_owner.stack_free(), r.manager.stack_free()});
    for (auto *endpoint : r.endpoints) {
        m.minimum_stack =
            std::min<uint32_t>({m.minimum_stack, endpoint->rx_stack_free(), endpoint->tx_stack_free()});
        if (endpoint->session().snapshot().phase == comms::Phase::Open)
            ++m.sessions;
    }
#ifdef CONFIG_RAVEN_BLE_ENABLED
    m.minimum_stack = std::min<uint32_t>({m.minimum_stack, r.ble_owner.stack_free(), r.ble_rx.stack_free()});
    const auto host_free = r.ble.host_stack_free();
    if (host_free)
        m.minimum_stack = std::min<uint32_t>(m.minimum_stack, host_free);
#endif
    m.stale = r.peer_state.stale_messages();
    m.diagnostic_drops = r.diagnostics.drops() + system_log_drops();
    return m;
}
static_assert(sizeof(runtime::Message) == 88 && sizeof(runtime::Inbox) == 1520,
              "Update target memory budget");
} // namespace
void start_device() {
    static WifiNetwork wifi;
    wifi.start(mission_computer, CONFIG_RAVEN_WIFI_SSID, CONFIG_RAVEN_WIFI_PASSWORD, session_count);
#ifdef CONFIG_RAVEN_TCP_BOARD_PROBE
    run_tcp_board_probe(wifi, CONFIG_RAVEN_TCP_PORT);
    return;
#endif
    static Runtime runtime(wifi);
    auto &r = runtime;
    capture_system_log(r.journal, r.local.device, r.local.boot);
    ESP_ERROR_CHECK(r.dispatcher.bind(peer_endpoint, r.peers.inbox) ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(r.dispatcher.bind(diagnostics_endpoint, r.diagnostics.inbox) ? ESP_OK : ESP_FAIL);
    for (size_t i = 0; i < session_count; ++i) {
        r.sessions[i].configure(uint32_t(i), uint8_t(i));
        r.endpoints[i] = new (&r.endpoint_storage[i]) TcpEndpoint(r.sessions[i], r.port);
        ESP_ERROR_CHECK(r.dispatcher.bind(uint8_t(i), r.endpoints[i]->inbox()) ? ESP_OK : ESP_FAIL);
    }
#ifdef CONFIG_RAVEN_BLE_ENABLED
    ESP_ERROR_CHECK(r.dispatcher.bind(ble_endpoint, r.ble_tx.inbox) ? ESP_OK : ESP_FAIL);
    r.ble.start("Raven-BLE");
    r.ble_owner.start();
    r.ble_rx.start();
#endif
    r.peer_owner.start();
    for (auto *endpoint : r.endpoints)
        endpoint->start();
    r.manager.start();
    r.diagnostic_owner.start();
    r.journal.append(diagnostics::Event::Boot, now_ms(), r.local.device, r.local.boot,
                     mission_computer ? "MC ready" : "node ready");
    ESP_LOGI("Budget", "Message=%u Inbox=%u Runtime=%u sessions=%u", unsigned(sizeof(runtime::Message)),
             unsigned(sizeof(runtime::Inbox)), unsigned(sizeof(Runtime)), unsigned(session_count));
    ESP_LOGI("Budget", "Session=%u PeerSlot=%u PeerRecord=%u TcpEndpoint=%u JournalRecord=%u",
             unsigned(sizeof(comms::Session)), unsigned(sizeof(services::PeerSlot)),
             unsigned(sizeof(services::PeerRecord)), unsigned(sizeof(TcpEndpoint)),
             unsigned(sizeof(diagnostics::Record)));
    const auto m = metrics(&r);
    ESP_LOGI("Budget", "internal-dma free=%lu min=%lu largest=%lu", (unsigned long)m.free_heap,
             (unsigned long)m.minimum_heap, (unsigned long)m.largest_block);
#ifdef CONFIG_RAVEN_VALIDATION_MONITOR
    ESP_ERROR_CHECK(
        xTaskCreate(
            [](void *context) {
                auto &device = *static_cast<Runtime *>(context);
                for (;;) {
                    vTaskDelay(pdMS_TO_TICKS(10000));
                    const auto sample = metrics(context);
                    ESP_LOGI("Validation",
                             "sessions=%lu free=%lu min=%lu largest=%lu stack=%lu stale=%lu drops=%lu",
                             (unsigned long)sample.sessions, (unsigned long)sample.free_heap,
                             (unsigned long)sample.minimum_heap, (unsigned long)sample.largest_block,
                             (unsigned long)sample.minimum_stack, (unsigned long)sample.stale,
                             (unsigned long)sample.diagnostic_drops);
                    for (size_t i = 0; i < device.peer_state.capacity(); ++i) {
                        services::PeerRecord peer{};
                        if (device.peer_state.peer(i, peer))
                            ESP_LOGI("Validation",
                                     "peer=%012llX boot=%016llX state=%s slot=%lu gen=%lu seen=%llu",
                                     (unsigned long long)peer.descriptor.device,
                                     (unsigned long long)peer.descriptor.boot, services::name(peer.status),
                                     (unsigned long)peer.route.slot, (unsigned long)peer.route.generation,
                                     (unsigned long long)peer.last_seen);
                    }
                }
            },
            "validation", 3072, &r, 1, nullptr) == pdPASS
            ? ESP_OK
            : ESP_ERR_NO_MEM);
#endif
}
} // namespace raven
