#include "raven_platform/ble_serial_server.hpp"
#include "raven_platform/clock.hpp"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_log.h"
#include <algorithm>
#include <cstring>

namespace raven {
namespace {
constexpr const char *TAG = "BleSerial";
const ble_uuid16_t service_uuid = BLE_UUID16_INIT(0xffe0);
const ble_uuid16_t serial_uuid = BLE_UUID16_INIT(0xffe1);
void checked(int result) { ESP_ERROR_CHECK(result == 0 ? ESP_OK : ESP_FAIL); }
} // namespace
BleSerialServer *BleSerialServer::instance_{};
void BleSerialServer::start(const char *name) {
    ESP_ERROR_CHECK(instance_ == nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);
    instance_ = this;
    name_ = name;
    rx_ = xQueueCreateStatic(8, sizeof(Packet), rx_storage_.data(), &rx_control_);
    tx_ = xQueueCreateStatic(16, sizeof(Frame), tx_storage_.data(), &tx_control_);
    ESP_ERROR_CHECK(rx_ && tx_ ? ESP_OK : ESP_ERR_NO_MEM);
    // NVS/PHY are initialized by platform bootstrap before either radio starts.
    ESP_ERROR_CHECK(nimble_port_init());
    // Vendor INFO emits several lines per notification; keep the UART console
    // useful and avoid making radio throughput depend on verbose serial logging.
    esp_log_level_set("NimBLE", ESP_LOG_WARN);
    ble_hs_cfg.sync_cb = sync;
    ble_hs_cfg.reset_cb = reset;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    checked(ble_svc_gap_device_name_set(name));
    auto &chr = characteristics_[0];
    chr.uuid = &serial_uuid.u;
    chr.access_cb = access;
    chr.arg = this;
    chr.flags =
        BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY;
    chr.val_handle = &value_handle_;
    services_[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services_[0].uuid = &service_uuid.u;
    services_[0].characteristics = characteristics_;
    checked(ble_gatts_count_cfg(services_));
    checked(ble_gatts_add_svcs(services_));
    ble_npl_callout_init(&timer_, nimble_port_get_dflt_eventq(), pump_event, this);
    nimble_port_freertos_init([](void *) {
        instance_->host_task_ = xTaskGetCurrentTaskHandle();
        nimble_port_run();
        instance_->host_task_ = nullptr;
        nimble_port_freertos_deinit();
    });
}
void BleSerialServer::sync() {
    checked(ble_hs_util_ensure_addr(0));
    checked(ble_hs_id_infer_auto(0, &instance_->address_type_));
    ESP_LOGI(TAG, "GATT serial value_handle=%u", unsigned(instance_->value_handle_));
    instance_->advertise();
    ble_npl_callout_reset(&instance_->timer_, ble_npl_time_ms_to_ticks32(5));
}
void BleSerialServer::reset(int reason) {
    instance_->ready_ = false;
    instance_->connection_ = BLE_HS_CONN_HANDLE_NONE;
    ++instance_->session_;
    ESP_LOGW(TAG, "host reset reason=%d", reason);
}
void BleSerialServer::advertise() {
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids16 = &service_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;
    fields.name = reinterpret_cast<const uint8_t *>(name_);
    fields.name_len = std::strlen(name_);
    fields.name_is_complete = 1;
    checked(ble_gap_adv_set_fields(&fields));
    ble_gap_adv_params params{};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    checked(ble_gap_adv_start(address_type_, nullptr, BLE_HS_FOREVER, &params, gap, this));
    ESP_LOGI(TAG, "BLE_READY name=%s service=FFE0 characteristic=FFE1", name_);
}
int BleSerialServer::gap(ble_gap_event *event, void *arg) {
    auto &self = *static_cast<BleSerialServer *>(arg);
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {
            ESP_LOGW(TAG, "connection failed status=%d", event->connect.status);
            self.advertise();
            break;
        }
        self.connection_ = event->connect.conn_handle;
        ++self.session_;
        ESP_LOGI(TAG, "connected session=%lu", static_cast<unsigned long>(self.session_.load()));
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected reason=%d", event->disconnect.reason);
        self.ready_ = false;
        self.connection_ = BLE_HS_CONN_HANDLE_NONE;
        ++self.session_;
        self.advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == self.value_handle_) {
            self.ready_ = event->subscribe.cur_notify != 0;
            if (!self.ready_)
                self.disconnect(self.session_);
            ESP_LOGI(TAG, "notifications=%u", unsigned(event->subscribe.cur_notify));
        }
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        self.advertise();
        break;
    default:
        break;
    }
    return 0;
}
int BleSerialServer::access(uint16_t, uint16_t, ble_gatt_access_ctxt *ctx, void *arg) {
    auto &self = *static_cast<BleSerialServer *>(arg);
    if (ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        constexpr char hello[] = "L0:Raven ready\n";
        return os_mbuf_append(ctx->om, hello, sizeof(hello) - 1) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctx->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return BLE_ATT_ERR_UNLIKELY;
    // Subscribe first so every accepted application command has a reply path.
    if (!self.ready())
        return BLE_ATT_ERR_UNLIKELY;
    Packet packet;
    packet.session = self.session_;
    const auto length = OS_MBUF_PKTLEN(ctx->om);
    if (length > packet.data.size() ||
        ble_hs_mbuf_to_flat(ctx->om, packet.data.data(), packet.data.size(), &packet.size)) {
        self.disconnect(packet.session);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (xQueueSend(self.rx_, &packet, 0) != pdTRUE) {
        // Never continue a byte stream after a missing fragment, including Write
        // Without Response where ATT cannot return a visible error to the sender.
        self.disconnect(packet.session);
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}
bool BleSerialServer::send(uint32_t session, const char *bytes, size_t size) {
    if (!ready() || session != session_ || size > tx_capacity)
        return false;
    Frame frame;
    frame.session = session;
    frame.size = size;
    std::memcpy(frame.data.data(), bytes, size);
    return xQueueSend(tx_, &frame, 0) == pdTRUE;
}
void BleSerialServer::pump_event(ble_npl_event *event) {
    auto &self = *static_cast<BleSerialServer *>(ble_npl_event_get_arg(event));
    self.pump();
    ble_npl_callout_reset(&self.timer_, std::max<ble_npl_time_t>(1, ble_npl_time_ms_to_ticks32(5)));
}
void BleSerialServer::pump() {
    if (connection_ != BLE_HS_CONN_HANDLE_NONE && failed_session_ == session_) {
        ready_ = false;
        failed_session_ = 0;
        ESP_LOGW(TAG, "closing session=%lu (buffer failure or notifications disabled)",
                 static_cast<unsigned long>(session_.load()));
        ble_gap_terminate(connection_, BLE_ERR_REM_USER_CONN_TERM);
    }
    if (sending_ && (active_.session != session_ || !ready_))
        sending_ = false;
    if (!sending_) {
        if (xQueueReceive(tx_, &active_, 0) != pdTRUE)
            return;
        if (!ready_ || active_.session != session_)
            return;
        sending_ = true;
        offset_ = 0;
        tx_deadline_ = now_ms() + 500;
    }
    for (unsigned i = 0; i < 4 && sending_; ++i) {
        const auto count = std::min<size_t>(active_.size - offset_, ble_att_mtu(connection_) - 3);
        auto *buffer = ble_hs_mbuf_from_flat(active_.data.data() + offset_, count);
        const int result =
            buffer ? ble_gatts_notify_custom(connection_, value_handle_, buffer) : BLE_HS_ENOMEM;
        // notify_custom always consumes the mbuf. Retain our own bytes until accepted.
        if (!result) {
            offset_ += count;
            if (offset_ == active_.size)
                sending_ = false;
        } else {
            if (now_ms() >= tx_deadline_) {
                disconnect(active_.session);
                sending_ = false;
            }
            break;
        }
    }
}
} // namespace raven
