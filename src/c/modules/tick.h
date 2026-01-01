#pragma once
#include <pebble.h>


typedef void (*TickUpdateDeviceData)(void);
void tick_handle_update();
void force_tick_now(TimeUnits units);
void modify_tick(TimeUnits additional_ticks, int for_seconds);
void tick_init(TickUpdateDeviceData update_device_data);
void tick_deinit();
