#include <stdarg.h>
#include "weather.h"
#include "config_data.h"
#include "device_data.h"

#define WEATHER_KEY 200
#define VERSION_KEY 199

#define RETRY_MS 60000

static AppTimer *s_refresh_handle = NULL;
static status_t s_save_result = (int)E_UNKNOWN;

static WeatherUpdated *s_callbacks = NULL;
static int s_callbacks_size = 0;
static struct WeatherData s_weather;
static int s_timer_seconds = -1;
static int s_forecast_range;
static int s_weather_mode;
static int s_temp_unit;
static int s_wind_unit;
static int s_precip_unit;
static bool s_device_connected = false;
static bool s_wrist_flick = true;

static void weather_refresh(void *data) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Requesting Weather");
    data_request(MESSAGE_KEY_REQ_WEATHER, true);
    s_refresh_handle = app_timer_register(RETRY_MS, weather_refresh, NULL);
}

static void reset_refresh_timer(int ms) {
    if (!s_refresh_handle || !app_timer_reschedule(s_refresh_handle, ms)) {
        s_refresh_handle = app_timer_register(ms, weather_refresh, NULL);
    }
}

static void publish_update() {
    for(int i = 0; i < s_callbacks_size; i++) {
        s_callbacks[i]();
    }
}

bool weather_inbox_received(DictionaryIterator *iter) {
    Tuple *kvp = dict_read_first(iter);
    bool all = true;
    while (kvp) {
        if (kvp->key == MESSAGE_KEY_CURRENT_TEMP)
            s_weather.current_temp = kvp->value->int16; 
        if (kvp->key == MESSAGE_KEY_CURRENT_COND)
            snprintf(s_weather.current_cond, sizeof(s_weather.current_cond), "%s", kvp->value->cstring);
        if (kvp->key == MESSAGE_KEY_CURRENT_PRECIP)
            s_weather.current_precip = kvp->value->int16; 
        if (kvp->key == MESSAGE_KEY_CURRENT_WIND)
            s_weather.current_wind = kvp->value->int16; 
        if (kvp->key == MESSAGE_KEY_FUTURE_COND)
            snprintf(s_weather.future_cond, sizeof(s_weather.future_cond), "%s", kvp->value->cstring);
        if (kvp->key == MESSAGE_KEY_FUTURE_MIN)
            s_weather.future_min = kvp->value->int16; 
        if (kvp->key == MESSAGE_KEY_FUTURE_MAX)
            s_weather.future_max = kvp->value->int16; 
        if (kvp->key == MESSAGE_KEY_FUTURE_PRECIP)
            s_weather.future_precip = kvp->value->int16;
        if (kvp->key == MESSAGE_KEY_FUTURE_WIND)
            s_weather.future_wind = kvp->value->int16;
        else all = false;
        kvp = dict_read_next(iter);
    }
    s_weather.timestamp = time(NULL);
    s_save_result = persist_write_data(WEATHER_KEY, &s_weather, sizeof(s_weather)); // TODO: retry
    reset_refresh_timer(s_timer_seconds * 1000);
    publish_update();
    return all;
}

static void cache_units() {
    const struct AppConfig *config = app_config();
    s_weather_mode = config->weather_mode;
    s_forecast_range = config->weather_forecast_range;
    s_temp_unit = config->weather_temp_unit;
    s_wind_unit = config->weather_wind_unit;
    s_precip_unit = config->weather_precip_unit;
} 

static bool should_schedule() {
    return !(!s_weather_mode || !s_device_connected || (s_weather_mode == 1 && !s_wrist_flick));
}

static int ms_until_refresh() {
    time_t expected = s_weather.timestamp + s_timer_seconds;
    time_t now = time(NULL);
    return expected < now ? -1 : (expected - now) * 1000;
}

static void reschedule_refresh(int future_ms) {
    if (future_ms <= 0) { // due now
        if (s_refresh_handle) app_timer_cancel(s_refresh_handle);
        weather_refresh(NULL);
    } else {
        reset_refresh_timer(future_ms);
    } 
}

static void pause_refresh() {
    if (s_refresh_handle) app_timer_cancel(s_refresh_handle);
    s_refresh_handle = NULL;
}

void weather_handle_config_update() {
    const struct AppConfig *config = app_config();

    bool should_update = false;
    bool update_now = false;
    if (
        s_timer_seconds != config->weather_refresh_rate ||
        s_wrist_flick != (config->wrist_flick > 0)
    ) {
        s_timer_seconds = config->weather_refresh_rate;
        s_wrist_flick = config->wrist_flick > 0;
        should_update = true;
    }

    if (
        s_weather_mode != config->weather_mode ||
        s_forecast_range != config->weather_forecast_range ||
        s_temp_unit != config->weather_temp_unit ||
        s_wind_unit != config->weather_wind_unit ||
        s_precip_unit != config ->weather_precip_unit
    ) {
        cache_units();
        should_update = true;
        update_now = true;
    }

    if (!should_schedule()) pause_refresh();
    else if (should_update) reschedule_refresh(update_now ? -1 : ms_until_refresh());
}

void weather_handle_device_update() {
    const struct DeviceData *data = device_data();
    if (s_device_connected == data->app_connection) return;
    
    s_device_connected = data->app_connection;
    if (!should_schedule()) pause_refresh();
    else reschedule_refresh(ms_until_refresh());
}

static void clear_callbacks() {
    if (s_callbacks) {
        free(s_callbacks);
        s_callbacks = NULL;
    }
}

void weather_init(int argc, ...) {
    if (persist_exists(VERSION_KEY) && persist_read_int(VERSION_KEY) == WEATHER_VERSION && persist_exists(WEATHER_KEY)) {
        persist_read_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
    }
    persist_write_int(VERSION_KEY, WEATHER_VERSION);
    
    s_callbacks_size = argc;
    va_list args;
    va_start(args, argc);
    clear_callbacks();
    s_callbacks = (WeatherUpdated *)malloc(argc * sizeof(WeatherUpdated));
    if (s_callbacks != NULL) {
        for (int i = 0; i < argc; i++) {
            *(s_callbacks + i) = va_arg(args, WeatherUpdated);
        }
    }
    s_timer_seconds = app_config()->weather_refresh_rate;
    s_wrist_flick = app_config()->wrist_flick;
    cache_units();
    s_device_connected = device_data()->app_connection;
    if (should_schedule()) reschedule_refresh(ms_until_refresh());
}

void weather_deinit() {
    clear_callbacks();
    s_callbacks_size = 0;
    if (s_refresh_handle != NULL) {
        app_timer_cancel(s_refresh_handle);
        s_refresh_handle = NULL;
    } 
}

const struct WeatherData *weather_data() {
    return &s_weather;
}
