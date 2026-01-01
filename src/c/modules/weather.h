#pragma once

#include <pebble.h>

#define WEATHER_VERSION 3

struct WeatherData {
    time_t timestamp;
    int16_t current_temp;
    char current_cond[40]; // at least 30: Light Thunderstorms With Hail
    int16_t current_precip;
    int16_t current_wind;
    char future_cond[40];
    int16_t future_min;
    int16_t future_max;
    int16_t future_precip;
    int16_t future_wind;
};

typedef void (*WeatherUpdated)();

bool weather_inbox_received(DictionaryIterator *iter);
void weather_handle_config_update();
void weather_handle_device_update();

void weather_init(int argc, ...);
void weather_deinit();
const struct WeatherData *weather_data();
