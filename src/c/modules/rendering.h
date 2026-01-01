#pragma once

#include <pebble.h>
#include "device_data.h"
#include "weather.h"

void render_weather_data(const struct WeatherData *data);
void rendering_tick_handler(struct tm *tick_time, TimeUnits units_changed, TimeUnits current_units);
void rendering_handle_wakeup(bool);
void rendering_handle_config_update();
void rendering_handle_device_data_update();
void rendering_handle_weather_update();
void main_window_create_register();
