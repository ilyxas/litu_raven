#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "nimble/nimble_npl.h"
#include <array>
#include <atomic>

namespace raven {
// HM-10 compatible GATT transport (FFE0/FFE1), with bounded copied RX/TX queues.
// NimBLE host context owns connection/MTU and sends every notification fragment.
// A gateway task consumes receive() and queues complete lines with send().
// Session generations prevent fragments/replies leaking across reconnects.
// One instance, application lifetime; no actuator or command semantics here.
class BleSerialServer {
  public:
    static constexpr size_t rx_capacity = 512, tx_capacity = 192;
    struct Packet {
        uint32_t session{};
        uint16_t size{};
        std::array<uint8_t, rx_capacity> data{};
    };
    void start(const char *name);
    bool receive(Packet &packet) { return xQueueReceive(rx_, &packet, 0) == pdTRUE; }
    bool send(uint32_t session, const char *bytes, size_t size);
    bool ready() const { return ready_ && failed_session_ != session_; }
    uint32_t session() const { return session_; }
    unsigned host_stack_free() const {
        const auto handle = host_task_.load();
        return handle ? unsigned(uxTaskGetStackHighWaterMark(handle)) : 0;
    }
    bool tx_idle() const { return uxQueueMessagesWaiting(tx_) == 0 && !sending_; }
    void disconnect(uint32_t session) { failed_session_ = session; }

  private:
    struct Frame {
        uint32_t session{};
        uint16_t size{};
        std::array<char, tx_capacity> data{};
    };
    static int access(uint16_t conn, uint16_t attr, ble_gatt_access_ctxt *ctx, void *arg);
    static int gap(ble_gap_event *event, void *arg);
    static void sync();
    static void reset(int reason);
    static void pump_event(ble_npl_event *event);
    void advertise();
    void pump();
    static BleSerialServer *instance_;
    const char *name_{};
    ble_gatt_chr_def characteristics_[2]{};
    ble_gatt_svc_def services_[2]{};
    ble_npl_callout timer_{};
    uint8_t address_type_{};
    uint16_t connection_{BLE_HS_CONN_HANDLE_NONE}, value_handle_{};
    Frame active_{};
    size_t offset_{};
    uint64_t tx_deadline_{};
    std::atomic<uint32_t> session_{0}, failed_session_{0};
    std::atomic<bool> ready_{false}, sending_{false};
    std::atomic<TaskHandle_t> host_task_{nullptr};
    QueueHandle_t rx_{}, tx_{};
    StaticQueue_t rx_control_{}, tx_control_{};
    std::array<uint8_t, 4 * sizeof(Packet)> rx_storage_{};
    std::array<uint8_t, 8 * sizeof(Frame)> tx_storage_{};
};
} // namespace raven
