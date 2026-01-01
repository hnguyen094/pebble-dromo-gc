#include "interaction.h"
#include "config_data.h"
#include "device_data.h"
#include "battery_saver.h"

#define BUFFER_WINDOW_MS 500

static ModifyTick s_modify_tick = NULL;
static TimeUnits s_wakeup_tick_unit = SECOND_UNIT;
static InteractionUpdateData s_update_data = NULL;
static int s_wrist_flick = 1;
static bool s_battery_saver = false;

// TODO: add debounce
static int s_taps = 0;
static time_t s_tap_timeout = 0;
static uint16_t s_tap_timeout_ms = 0;

static void accel_tap_handler(AccelAxisType axis, int32_t direction) {
    time_t now;
    int16_t now_ms = time_ms(&now, NULL);
    
    bool in_window = s_tap_timeout > now || (s_tap_timeout == now && s_tap_timeout_ms > now_ms);
    s_taps = in_window ? (s_taps + 1) : 1;
    s_tap_timeout_ms = (now_ms + BUFFER_WINDOW_MS) % 1000;
    s_tap_timeout = now + ((now_ms + BUFFER_WINDOW_MS) >= 1000 ? 1 : 0);
    if (s_taps < s_wrist_flick) return;
    
    s_taps = 0;

    if (s_modify_tick != NULL)
        s_modify_tick(s_wakeup_tick_unit, app_config()->tap_wakeup_duration); 

    if (s_update_data != NULL) s_update_data();
}

static bool should_subscribe() {
    return s_wrist_flick && !s_battery_saver;
}

void interaction_handle_update() {
    const struct AppConfig *config = app_config();
    bool new_bat_value = battery_saver_is_active();
    s_wakeup_tick_unit = config->wakeup_tick_units;
    if (s_wrist_flick != config->wrist_flick || new_bat_value != s_battery_saver) {
        s_wrist_flick = config->wrist_flick;
        s_battery_saver = new_bat_value;
        if (should_subscribe()) {
            accel_tap_service_subscribe(accel_tap_handler);
        } else {
            accel_tap_service_unsubscribe();
        }
    }
}

void interaction_service_subscribe(ModifyTick handler, InteractionUpdateData update_data) {
    s_modify_tick = handler;
    s_update_data = update_data;
    s_wrist_flick = app_config()->wrist_flick;
    s_wakeup_tick_unit = app_config()->wakeup_tick_units;
    time_ms(&s_tap_timeout, &s_tap_timeout_ms);
    s_tap_timeout -= 10; // heuristic so any potential diff don't overflow.
    if (should_subscribe()) {
        accel_tap_service_subscribe(accel_tap_handler);
    }
}

void interaction_service_unsubscribe() {
    s_modify_tick = NULL;
    s_update_data = NULL;
    if (should_subscribe()) {
        accel_tap_service_unsubscribe();
    }
}
