#pragma once
#define CONFIG_VERSION 16

#include <pebble.h>

struct AppConfig {
    TimeUnits tick_units;
    TimeUnits qtm_tick_units;
    TimeUnits wakeup_tick_units;
    GColor accent_color;
    int tap_wakeup_duration;
    int health_mode;
    int weather_mode;
    int bat_mode;
    int bat_mode_lb;
    int bat_mode_st_start;
    int bat_mode_st_end;
    int weather_refresh_rate;
    int wrist_flick;
    int hour_mode;
    int tz_mode;
    int tz_id;
    int tz_offset;
    char tz_code[7];
    bool subscribe_to_data;
    bool inverted_timebox;
    bool battery_percentage;
    bool wakeup_light;
 
    // the following are inefficient.
    // Clay should handle this, or these values should be packed.
    int weather_forecast_range;
    int weather_temp_unit;
    int weather_wind_unit;
    int weather_precip_unit;
}; // TODO: realign the bytes, assert(sizeof(AppConfig))

typedef void (*ConfigUpdated)();
typedef bool (*InboxReceived)(DictionaryIterator *iter);

void data_request(uint32_t request_key, bool retry);

void config_data_init(InboxReceived inbox_received, int argc, ...);
void config_data_deinit();

const struct AppConfig *app_config();
