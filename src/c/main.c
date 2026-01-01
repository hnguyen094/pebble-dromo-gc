#include <pebble.h>

#include "modules/rendering.h"
#include "modules/tick.h"
#include "modules/config_data.h"
#include "modules/device_data.h"
#include "modules/interaction.h"
#include "modules/weather.h"
#include "modules/battery_saver.h"

static void prv_init(void) {
    config_data_init(
        weather_inbox_received,
        6,
        device_data_handle_config_update,
        battery_saver_handle_update,
        tick_handle_update,
        interaction_handle_update,
        rendering_handle_config_update,
        weather_handle_config_update
    );
    device_data_init(4, 
        weather_handle_device_update, 
        rendering_handle_device_data_update, 
        tick_handle_update, 
        interaction_handle_update
    );
    battery_saver_init(3,
        device_data_handle_config_update,
        interaction_handle_update,
        tick_handle_update
    );
    weather_init(1, rendering_handle_weather_update);
    main_window_create_register();

    interaction_service_subscribe(modify_tick, update_device_data);
    
    update_device_data();
    tick_init(update_device_data);

    APP_LOG(APP_LOG_LEVEL_DEBUG, "heap used/free after init: %d/%d", heap_bytes_used(), heap_bytes_free());
}

static void prv_deinit(void) {
    config_data_deinit();
    tick_deinit();
    interaction_service_unsubscribe();
    weather_deinit();
    battery_saver_deinit();
    device_data_deinit();
}

int main(void) {
    prv_init();
    app_event_loop();
    prv_deinit();
}
