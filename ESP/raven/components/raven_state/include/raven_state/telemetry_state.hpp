#pragma once

#include <cstdint>
#include <mutex>

namespace raven {

// TelemetryState — thread-safe shared store for categorised telemetry data.
//
// Services write into each category; consumers (e.g. TelemetryService) read
// consistent snapshots.  Every field group is protected by its own mutex so
// that writers in different categories do not contend with one another.
// Atomics are intentionally avoided; all protection is handled by std::mutex.
struct TelemetryState {

    struct NavigationTelemetry {
        float    speed{0.0f};
        float    direction{0.0f};
        float    position_x{0.0f};
        float    position_y{0.0f};
        uint32_t timestamp{0};
    };

    void set_navigation(const NavigationTelemetry& data)
    {
        std::unique_lock<std::mutex> lock(nav_mutex_);
        nav_ = data;
    }

    NavigationTelemetry get_navigation() const
    {
        std::unique_lock<std::mutex> lock(nav_mutex_);
        return nav_;
    }

    struct SystemTelemetry {
        int32_t  rssi{0};
        uint32_t heap_free{0};
        float    battery_pct{0.0f};
        uint32_t timestamp{0};
    };

    void set_system(const SystemTelemetry& data)
    {
        std::unique_lock<std::mutex> lock(sys_mutex_);
        sys_ = data;
    }

    SystemTelemetry get_system() const
    {
        std::unique_lock<std::mutex> lock(sys_mutex_);
        return sys_;
    }

    struct SensorTelemetry {
        float    lidar_distance{0.0f};
        float    humidity{0.0f};
        float    temperature{0.0f};
        float    noise_level{0.0f};
        uint32_t timestamp{0};
    };

    void set_sensor(const SensorTelemetry& data)
    {
        std::unique_lock<std::mutex> lock(sensor_mutex_);
        sensor_ = data;
    }

    SensorTelemetry get_sensor() const
    {
        std::unique_lock<std::mutex> lock(sensor_mutex_);
        return sensor_;
    }

private:
    mutable std::mutex nav_mutex_;
    NavigationTelemetry nav_;

    mutable std::mutex sys_mutex_;
    SystemTelemetry sys_;

    mutable std::mutex sensor_mutex_;
    SensorTelemetry sensor_;
};

} // namespace raven
