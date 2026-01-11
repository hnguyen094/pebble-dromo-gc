#include "config_data.h"
#include <stdarg.h>

#define CONFIG_KEY 100
#define VERSION_KEY 99

#define OUTBOX_TIMEOUT 1000
#define OUTBOX_RETRY 1000 

static struct AppConfig s_app_config = {
    .tick_units = -1 ^ SECOND_UNIT,
    .qtm_tick_units = ~(HOUR_UNIT - 1),
    .wakeup_tick_units = -1,
    .tap_wakeup_duration = 5,
    .subscribe_to_data = true,
    .wrist_flick = 1,
    .accent_color = {0},
    .inverted_timebox = false,
    .battery_percentage = false,
    .wakeup_light = true,
    .hour_mode = 2,
    .tz_mode = 1,
    .tz_id = 0,
    .tz_offset = 0,
    .weather_refresh_rate = 3600,
    .weather_forecast_range = 24,
    .weather_temp_unit = 1,
    .weather_wind_unit = 2,
    .weather_precip_unit = 1,
    .weather_mode = 0,
    .health_mode = 1,
    .bat_mode = 1,
    .bat_mode_st_start = 23,
    .bat_mode_st_end = 7,
    .bat_mode_lb = 10
};
static AppMessageResult s_open_result = APP_MSG_APP_NOT_RUNNING;
static status_t s_save_result = (int)E_UNKNOWN;

static ConfigUpdated *s_config_updated_callbacks = NULL;
static int s_config_updated_callbacks_size = 0;

static InboxReceived s_inbox_received = NULL; 
static AppTimer *s_timeout_handle = NULL;
static int s_outbox_attempts = 0;

static void config_updated() {
    for(int i = 0; i < s_config_updated_callbacks_size; i++) {
        s_config_updated_callbacks[i]();
    }
}

static bool config_received(DictionaryIterator *iter) {
    Tuple *kvp = dict_read_first(iter);
    bool read_all = true;
    while (kvp) {
        if (kvp->key == MESSAGE_KEY_SUBSCRIBE_TO_DATA) 
            s_app_config.subscribe_to_data = (bool)kvp->value->int16;
        else if (kvp->key == MESSAGE_KEY_TICK_UNITS) 
            s_app_config.tick_units = (TimeUnits)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_QTM_TICK_UNITS) 
            s_app_config.qtm_tick_units = (TimeUnits)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WAKEUP_TICK_UNITS) 
            s_app_config.wakeup_tick_units = (TimeUnits)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_TAP_WAKEUP_DURATION) 
            s_app_config.tap_wakeup_duration = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WEATHER_REFRESH_RATE) 
            s_app_config.weather_refresh_rate = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WEATHER_MODE) 
            s_app_config.weather_mode = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_HEALTH_MODE) 
            s_app_config.health_mode = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_BAT_MODE) 
            s_app_config.bat_mode = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_BAT_MODE_LB) 
            s_app_config.bat_mode_lb = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_BAT_MODE_ST_START) 
            s_app_config.bat_mode_st_start = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_BAT_MODE_ST_END) 
            s_app_config.bat_mode_st_end = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WRIST_FLICK) 
            s_app_config.wrist_flick = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_INVERTED_TIMEBOX) 
            s_app_config.inverted_timebox = (bool)kvp->value->int16;
        else if (kvp->key == MESSAGE_KEY_ACCENT_COLOR) 
            s_app_config.accent_color = GColorFromHEX(kvp->value->int32);
        else if (kvp->key == MESSAGE_KEY_BATTERY_PERCENTAGE) 
            s_app_config.battery_percentage = (bool)kvp->value->int16;
        else if (kvp->key == MESSAGE_KEY_WAKEUP_LIGHT) 
            s_app_config.wakeup_light = (bool)kvp->value->int16;
        else if (kvp->key == MESSAGE_KEY_HOUR_MODE) 
            s_app_config.hour_mode = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_TZ_MODE) 
            s_app_config.tz_mode = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_TZ_ID) 
            s_app_config.tz_id = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_TZ_OFFSET) 
            s_app_config.tz_offset = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_TZ_CODE) 
            strcpy(s_app_config.tz_code, kvp->value->cstring);
        else if (kvp->key == MESSAGE_KEY_WEATHER_FORECAST_RANGE)
            s_app_config.weather_forecast_range = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WEATHER_TEMP_UNIT)
            s_app_config.weather_temp_unit = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WEATHER_WIND_UNIT)
            s_app_config.weather_wind_unit = (int)kvp->value->int32;
        else if (kvp->key == MESSAGE_KEY_WEATHER_PRECIP_UNIT)
            s_app_config.weather_precip_unit = (int)kvp->value->int32;
        else {
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Not Reading %d", (int)kvp->key);
            read_all = false;
        }
        kvp = dict_read_next(iter);
    }
    config_updated();
    s_save_result = persist_write_data(CONFIG_KEY, &s_app_config, sizeof(s_app_config));
    return read_all;
}

static void data_request_retry(void *data) {
    s_timeout_handle = NULL;
    data_request((uint32_t)data, true);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
    if (!config_received(iter) && s_inbox_received) {
        s_inbox_received(iter);
    }
}

static void inbox_dropped(AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Inbox dropped message. (%d)", (int)reason); 
}

static void outbox_sent(DictionaryIterator *iter, void *context) {
    if (s_timeout_handle) app_timer_cancel(s_timeout_handle);
    s_timeout_handle = NULL;
    app_message_set_context(NULL);
    s_outbox_attempts = 0;
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox failed: %d", (int)reason);
    if (!s_timeout_handle || !app_timer_reschedule(s_timeout_handle, OUTBOX_RETRY)) {
        s_timeout_handle = app_timer_register(OUTBOX_RETRY, data_request_retry, context);
    }
}

static void message_open() {
    s_open_result = app_message_open(300, APP_MESSAGE_OUTBOX_SIZE_MINIMUM);
}

static void clear_callbacks() {
    if (s_config_updated_callbacks) {
        free(s_config_updated_callbacks);
        s_config_updated_callbacks = NULL;
    }
}

void data_request(uint32_t request_key, bool retry) {
    if (s_open_result != APP_MSG_OK) message_open(); // try again 
    if (s_timeout_handle) {
        app_timer_cancel(s_timeout_handle);
    }
    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK) {
        DictionaryResult res = dict_write_uint8(iter, request_key, 0); 
        if (request_key == MESSAGE_KEY_REQ_WEATHER) {
            res |= dict_write_int32(iter, MESSAGE_KEY_WEATHER_FORECAST_RANGE, s_app_config.weather_forecast_range);
            res |= dict_write_int32(iter, MESSAGE_KEY_WEATHER_TEMP_UNIT, s_app_config.weather_temp_unit);
            res |= dict_write_int32(iter, MESSAGE_KEY_WEATHER_WIND_UNIT, s_app_config.weather_wind_unit);
            res |= dict_write_int32(iter, MESSAGE_KEY_WEATHER_PRECIP_UNIT, s_app_config.weather_precip_unit);
        }
        if (res != DICT_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to construct dictionary for outbox. %d (%d)", res, s_outbox_attempts);
        }
        app_message_set_context((void *)request_key);
        result |= app_message_outbox_send();
    } else if (result & APP_MSG_BUSY && s_outbox_attempts > 1) {
        // s_open_result = APP_MSG_APP_NOT_RUNNING;
        // TODO: something must be done here.
    }
    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Failed to begin/send outbox. %d (%d)", result, s_outbox_attempts);
    }
    if (result & APP_MSG_NOT_CONNECTED || result & APP_MSG_APP_NOT_RUNNING) {
        return; // no amount of retrying is going to help-- let the caller handle watching the device state. 
    }
    if (retry) {
        s_outbox_attempts += 1;
        s_timeout_handle = app_timer_register(OUTBOX_TIMEOUT, data_request_retry, (void *)request_key);
    }
}

void config_data_init(InboxReceived additional_inbox_received, int argc, ...) {
    s_inbox_received = additional_inbox_received;
    s_app_config.accent_color = GColorYellow;
    if (persist_exists(VERSION_KEY) && persist_read_int(VERSION_KEY) == CONFIG_VERSION && persist_exists(CONFIG_KEY)) {
        persist_read_data(CONFIG_KEY, &s_app_config, sizeof(s_app_config));
    }
    persist_write_int(VERSION_KEY, CONFIG_VERSION);
    
    s_config_updated_callbacks_size = argc;
    va_list args;
    va_start(args, argc);
    clear_callbacks();
    s_config_updated_callbacks = (ConfigUpdated *)malloc(argc * sizeof(ConfigUpdated));
    if (s_config_updated_callbacks != NULL) { 
        for (int i = 0; i < argc; i++) {
           *(s_config_updated_callbacks + i) = va_arg(args, ConfigUpdated);
        }
    }

    app_message_register_inbox_received(inbox_received);
    app_message_register_inbox_dropped(inbox_dropped);
    app_message_register_outbox_sent(outbox_sent);
    app_message_register_outbox_failed(outbox_failed);
    
    message_open();
}

void config_data_deinit() {
    s_inbox_received = NULL;
    s_open_result = APP_MSG_APP_NOT_RUNNING;
    clear_callbacks();
    s_config_updated_callbacks_size = 0;
    if (s_outbox_attempts > 0) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Application exited with %d outbox attempts failed.", s_outbox_attempts);
        s_outbox_attempts = 0;
    }
}

const struct AppConfig *app_config() {
    if (s_open_result != APP_MSG_OK) message_open(); // try again 
    return &s_app_config;
}
