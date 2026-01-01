#pragma once
#include <pebble.h>

typedef void (*ModifyTick)(TimeUnits additional_units, int for_seconds);
typedef void (*InteractionUpdateData)(void);

void interaction_handle_update();
void interaction_service_subscribe(ModifyTick modify_tick, InteractionUpdateData update_data);
void interaction_service_unsubscribe();
