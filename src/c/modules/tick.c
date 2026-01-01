#include "tick.h"
#include "rendering.h"
#include "config_data.h"
#include "device_data.h"
#include "battery_saver.h"

static TimeUnits s_tick_units = -1;
static TimeUnits s_qtm_tick_units = -1;
static TimeUnits s_current_tick_units = -1;
static TickUpdateDeviceData s_update_device_data = NULL;
static bool s_battery_saver = false;

static AppTimer *s_tick_restore_handle = NULL;

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
    if (s_update_device_data) s_update_device_data();
    if (!units_changed) {
        units_changed = -1; // I think this is correct.
    }
    rendering_tick_handler(tick_time, units_changed, s_current_tick_units);
}

static TimeUnits default_tick() {
    return battery_saver_is_active() ? s_qtm_tick_units : s_tick_units;
} 

void force_tick_now(TimeUnits units) {
    time_t current = time(NULL);
    tick_handler(localtime(&current), units);
}

void tick_handle_update() {
    bool new_bat_value = battery_saver_is_active();
    if (
        s_tick_units == app_config()->tick_units &&
        s_qtm_tick_units == app_config()->qtm_tick_units &&
        s_battery_saver == new_bat_value
    ) return;
    s_tick_units = app_config()->tick_units;
    s_qtm_tick_units = app_config()->qtm_tick_units;
    s_battery_saver = new_bat_value;
    if (!s_tick_restore_handle) {
        tick_timer_service_subscribe(s_current_tick_units = default_tick(), tick_handler);
        force_tick_now(-1);
    }
}

static void tick_restore(void *data) {
    tick_timer_service_subscribe(s_current_tick_units = default_tick(), tick_handler);
    s_tick_restore_handle = NULL;
    if (app_config()->wakeup_light) light_enable(false);
    rendering_handle_wakeup(false);
}

// Overrides the previous change request.
void modify_tick(TimeUnits tick_units, int for_seconds) {
    int for_ms = for_seconds * 1000; 
    if (!s_tick_restore_handle || !app_timer_reschedule(s_tick_restore_handle, for_ms)) {
        s_tick_restore_handle = app_timer_register(for_ms, tick_restore, NULL);
        if (app_config()->wakeup_light) light_enable(true);
    }
    TimeUnits new_units = default_tick() | tick_units;
    if (s_current_tick_units != new_units) { 
        tick_timer_service_subscribe(s_current_tick_units = new_units, tick_handler); 
        force_tick_now(-1);
    }
    rendering_handle_wakeup(true);
}

void tick_init(TickUpdateDeviceData update_device_data) {
    s_update_device_data = update_device_data;
    s_tick_units = app_config()->tick_units;
    s_qtm_tick_units = app_config()->qtm_tick_units;
    tick_timer_service_subscribe(s_current_tick_units = default_tick(), tick_handler);
    force_tick_now(-1);
}

void tick_deinit() {
    s_update_device_data = NULL; 
    if (s_tick_restore_handle != NULL) {
        app_timer_cancel(s_tick_restore_handle);
        s_tick_restore_handle = NULL;
    }
    tick_timer_service_unsubscribe();
}
