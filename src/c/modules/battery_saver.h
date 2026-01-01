#pragma once

#include <pebble.h>

typedef void (*BatterySaverUpdated)();

void battery_saver_handle_update();
void battery_saver_init(int argc, ...);
void battery_saver_deinit();

bool battery_saver_is_active();
