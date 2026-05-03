#pragma once

#include "raven_gateways/encoder.hpp"
#include "raven_gateways/network_header.hpp"
#include "raven_services/telemetry_messages.hpp"

#include "cJSON.h"

#include <cstring>
#include <vector>

namespace raven {

// TelemetryJsonEncoder — serialises a TelemetrySnapshot to a JSON wire frame.
//
// Expects msg.data to point to a TelemetrySnapshot (payload_size must be at
// least sizeof(TelemetrySnapshot)).  Produces a Raven wire frame
// (NetworkHeader + UTF-8 JSON payload) ready for a single TCP write.
//
// Thread-safety: same contract as RawEncoder — do not share across tasks.
class TelemetryJsonEncoder final : public IEncoder {
public:
    bool encode(const TaskMessage& msg, EncodedFrame& out) const override
    {
        if (msg.data == nullptr || msg.payload_size < sizeof(TelemetrySnapshot)) {
            return false;
        }

        const auto* s = static_cast<const TelemetrySnapshot*>(msg.data);

        cJSON* root = cJSON_CreateObject();
        if (!root) return false;

        cJSON_AddNumberToObject(root, "sent_ts", s->sent_ts);

        cJSON* nav = cJSON_AddObjectToObject(root, "navigation");
        if (!nav) { cJSON_Delete(root); return false; }
        cJSON_AddNumberToObject(nav, "speed",      s->navigation.speed);
        cJSON_AddNumberToObject(nav, "direction",  s->navigation.direction);
        cJSON_AddNumberToObject(nav, "position_x", s->navigation.position_x);
        cJSON_AddNumberToObject(nav, "position_y", s->navigation.position_y);
        cJSON_AddNumberToObject(nav, "ts",         s->navigation.ts);

        cJSON* sys = cJSON_AddObjectToObject(root, "system");
        if (!sys) { cJSON_Delete(root); return false; }
        cJSON_AddNumberToObject(sys, "rssi",        s->system.rssi);
        cJSON_AddNumberToObject(sys, "heap_free",   s->system.heap_free);
        cJSON_AddNumberToObject(sys, "battery_pct", s->system.battery_pct);
        cJSON_AddNumberToObject(sys, "ts",          s->system.ts);

        cJSON* sensors = cJSON_AddObjectToObject(root, "sensors");
        if (!sensors) { cJSON_Delete(root); return false; }
        cJSON_AddNumberToObject(sensors, "lidar_distance", s->sensors.lidar_distance);
        cJSON_AddNumberToObject(sensors, "humidity",       s->sensors.humidity);
        cJSON_AddNumberToObject(sensors, "temperature",    s->sensors.temperature);
        cJSON_AddNumberToObject(sensors, "noise_level",    s->sensors.noise_level);
        cJSON_AddNumberToObject(sensors, "ts",             s->sensors.ts);

        char* json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);

        if (!json_str) return false;

        const size_t json_len   = strlen(json_str);
        const size_t frame_size = sizeof(NetworkHeader) + json_len;
        buffer_.resize(frame_size);

        auto* hdr         = reinterpret_cast<NetworkHeader*>(buffer_.data());
        hdr->msg_id       = msg.id;
        hdr->payload_size = static_cast<uint32_t>(json_len);

        std::memcpy(buffer_.data() + sizeof(NetworkHeader), json_str, json_len);
        cJSON_free(json_str);

        out.data = buffer_.data();
        out.size = frame_size;
        return true;
    }

private:
    mutable std::vector<uint8_t> buffer_;
};

} // namespace raven
