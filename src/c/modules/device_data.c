#include <stdarg.h>
#include "device_data.h"
#include "config_data.h"
#include "battery_saver.h"
// note that all health endpoints will return reasonable defaults when PBL_HEALTH is not defined.
// Therefore, it is enough to only use #if defined(PBL_HEALTH) and the _accessible endpoints when we have a correct way to handle the failure.
// An example of correctly handling the failure is going to be near the rendering side: to display to the user that the capability is not available.
//
// The extra function calls do have some minor cost, but we'd rather have the readability. 

#define QTM_POLLING_MS 30000 // 30s

static bool s_subscribe_to_data = false;
static int s_health_mode = 0;
static bool s_health_subscribed = false; 
static DataUpdated *s_callbacks = NULL;
static int s_callbacks_size = 0;
static AppTimer *s_qtm_handle = NULL;
static bool s_battery_saver = false;

static struct DeviceData s_data;

static void publish_update() {
    for(int i = 0; i < s_callbacks_size; i++) {
        s_callbacks[i]();
    }
}

static void app_connection_handler(bool connected) {
    s_data.app_connection = connected;
    publish_update();
}

static void pk_connection_handler(bool connected) {
    s_data.pk_connection = connected;
    publish_update();
}

static void battery_handler(BatteryChargeState state) {
    s_data.battery = state;
    publish_update();
}

static void health_handler(HealthEventType event, void *context) {
    switch (event) {
        case HealthEventSignificantUpdate:
        case HealthEventMovementUpdate:
            s_data.steps_today = health_service_sum_today(HealthMetricStepCount);
            s_data.activities = health_service_peek_current_activities();
            break;
        case HealthEventSleepUpdate:
            s_data.activities = health_service_peek_current_activities();
            break;
        case HealthEventMetricAlert:
        case HealthEventHeartRateUpdate:
            s_data.heart_rate = health_service_peek_current_value(HealthMetricHeartRateBPM);
            break;
    }
    publish_update();
}

static void quiet_time_handler(void *data) {
    int new_qtm = quiet_time_is_active();
    if (s_data.quiet_time != new_qtm) {
        s_data.quiet_time = new_qtm;
        publish_update();
    }
    s_qtm_handle = app_timer_register(QTM_POLLING_MS, quiet_time_handler, NULL);
}

static void force_update_device_data() {
    s_data.app_connection = connection_service_peek_pebble_app_connection();
    s_data.pk_connection = connection_service_peek_pebblekit_connection();
    s_data.quiet_time = quiet_time_is_active();
    s_data.battery = battery_state_service_peek();
    s_data.steps_today = health_service_sum_today(HealthMetricStepCount);

    if (s_health_mode) {
        s_data.activities = health_service_peek_current_activities();
        s_data.heart_rate = health_service_peek_current_value(HealthMetricHeartRateBPM);
    }
}

static bool should_subscribe() {
    return s_subscribe_to_data && !s_battery_saver; 
}

static void subscribe_all() {
    connection_service_subscribe( (ConnectionHandlers) {
        .pebble_app_connection_handler = app_connection_handler,
        .pebblekit_connection_handler = pk_connection_handler,
    }); // silly subscription doesn't run unless you peek at it.
    battery_state_service_subscribe(battery_handler);
    s_qtm_handle = app_timer_register(QTM_POLLING_MS, quiet_time_handler, NULL);
    
    if (!s_health_mode) return;
    s_health_subscribed = health_service_events_subscribe(health_handler, NULL);
    // health_service_register_metric_alert(HealthMetricHeartRateBPM, /*more or less than 25/75% day average*/);
}

static void clear_callbacks() {
    if (s_callbacks) {
        free(s_callbacks);
        s_callbacks = NULL;
    }
}

static void unsubscribe_all() {
    connection_service_unsubscribe();
    battery_state_service_unsubscribe();
    if (s_qtm_handle) {
        app_timer_cancel(s_qtm_handle);
        s_qtm_handle = NULL;
    }

    if (s_health_mode) {
        health_service_events_unsubscribe();
        s_health_subscribed = false;
    }
}

void device_data_handle_config_update() {
    const struct AppConfig *config = app_config();
    bool new_bat_value = battery_saver_is_active();
    if (
        s_subscribe_to_data != config->subscribe_to_data || 
        s_health_mode != config->health_mode ||
        s_battery_saver != new_bat_value
    ) {
        s_subscribe_to_data = config->subscribe_to_data;
        s_health_mode = config->health_mode;
        s_battery_saver = new_bat_value;
        force_update_device_data();
    }
    if (should_subscribe()) {
        subscribe_all();
    } else {
        publish_update();
        unsubscribe_all();
    }
}

void device_data_init(int argc, ...) {
    s_callbacks_size = argc;
    va_list args;
    va_start(args, argc);
    clear_callbacks();
    s_callbacks = (DataUpdated *)malloc(argc * sizeof(DataUpdated));
    if (s_callbacks != NULL) {
        for (int i = 0; i < argc; i++) {
            *(s_callbacks + i) = va_arg(args, DataUpdated);
        }
    }
    force_update_device_data();
    s_subscribe_to_data = app_config()->subscribe_to_data;
    s_health_mode = app_config()->health_mode;
    if (should_subscribe()) subscribe_all();
}

void device_data_deinit() {
    clear_callbacks();
    s_callbacks_size = 0;
    if (should_subscribe()) unsubscribe_all();
}

const struct DeviceData *device_data() {
    return &s_data;
}

void update_device_data() {
    if (should_subscribe()) {
#if defined(PBL_HEALTH)
        if (!s_health_subscribed && s_health_mode) { // retry subscription
            s_health_subscribed = health_service_events_subscribe(health_handler, NULL); 
        }
        if (s_health_subscribed) return;
#else
        return;
#endif
    }
    force_update_device_data();
    publish_update();
}
