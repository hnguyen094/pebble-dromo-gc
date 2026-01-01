#pragma once

#include <pebble.h>

struct DeviceData {
    bool app_connection;
    bool pk_connection;
    bool quiet_time;
    BatteryChargeState battery;
    HealthValue steps_today;
    HealthValue heart_rate;
    HealthActivityMask activities;
};

typedef void (*DataUpdated)();

void device_data_handle_config_update();
void device_data_init(int argc, ...);
void device_data_deinit();
const struct DeviceData *device_data();
void update_device_data(); 
