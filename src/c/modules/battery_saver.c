#include <stdarg.h>
#include "battery_saver.h"
#include "config_data.h"

#define POLLING_MS 60000 // 60s

enum BatteryMode {
    BM_LB = 1,
    BM_ST = 1 << 1,
    BM_QT = 1 << 2
};

static AppTimer *s_handle = NULL;
static AppTimer *s_st_handle = NULL;
static int s_bat_mode = 0;
static int s_st_start = -1;
static int s_st_end = -1;
static BatterySaverUpdated *s_callbacks = NULL;
static int s_callbacks_size = 0;
static bool s_active = false;

static void publish_update() {
    for(int i = 0; i < s_callbacks_size; i++) {
        s_callbacks[i]();
    }
}

static bool is_active() {
    const struct AppConfig *config = app_config();
    BatteryChargeState battery = battery_state_service_peek();
    bool quiet_time = quiet_time_is_active();

    bool lb = s_bat_mode & BM_LB && battery.charge_percent <= config->bat_mode_lb;
    
    time_t now = time(NULL);
    struct tm *time = localtime(&now);
    int hour = time->tm_hour;

    bool st = s_bat_mode & BM_ST;
    if (s_st_start < s_st_end) {
        st = st && (s_st_start <= hour && hour < s_st_end);
    } else {
        st = st && (hour >= s_st_start || hour < s_st_end);
    }

    bool qt = s_bat_mode & BM_QT && quiet_time;

    return lb || st || qt;
}

static void battery_saver_update(bool publish) {
    bool new_value = is_active();
    if (s_active != new_value) {
        s_active = new_value;
        if (publish) publish_update();
    }
}

static int future_ms() {
    time_t day_start = time_start_of_today();
    int start = day_start + s_st_start * SECONDS_PER_HOUR;
    int end = day_start + s_st_end * SECONDS_PER_HOUR;
    int sooner = start < end ? start : end;
    int later = start < end ? end: start;
    time_t now = time(NULL);
    time_t target = now < sooner ? sooner : now < later ? later: (sooner + SECONDS_PER_DAY);

    return (target-now) * 1000;

}

static void st_handler(void *data) {
    s_st_handle = NULL;
    int reschedule_ms = future_ms();
    if (!s_st_handle || !app_timer_reschedule(s_st_handle, reschedule_ms)) {
        s_st_handle = app_timer_register(reschedule_ms, st_handler, NULL);
    }
    battery_saver_update(true);
}

static void handler(void *data) {
    battery_saver_update(data == NULL);
    s_handle = app_timer_register(POLLING_MS, handler, NULL);
}

void battery_saver_handle_update() {
    const struct AppConfig *config = app_config();
    if (
            s_st_start != config->bat_mode_st_start || 
            s_st_end != config->bat_mode_st_end ||
            s_bat_mode != config->bat_mode
    ) {
        s_bat_mode = config->bat_mode;
        s_st_start = config->bat_mode_st_start;
        s_st_end = config->bat_mode_st_end;

        if (s_bat_mode & BM_ST) {
            int reschedule_ms = future_ms();
            if (!s_st_handle || !app_timer_reschedule(s_st_handle, reschedule_ms)) {
                s_st_handle = app_timer_register(reschedule_ms, st_handler, NULL);
            }
        } else if (s_st_handle) {
            app_timer_cancel(s_st_handle);
            s_st_handle = NULL;
        }

        if (s_bat_mode & BM_QT) {
            if (!s_handle || !app_timer_reschedule(s_handle, POLLING_MS)) {
                s_handle = app_timer_register(POLLING_MS, handler, NULL);
            }
        } else if (s_handle) {
            app_timer_cancel(s_handle);
            s_handle = NULL;
        }
    }
    
    battery_saver_update(false);
}

void battery_saver_init(int argc, ...) {
    s_callbacks_size = argc;
    va_list args;
    va_start(args, argc);
    s_callbacks = (BatterySaverUpdated *)malloc(argc * sizeof(BatterySaverUpdated));
    if (s_callbacks != NULL) {
        for (int i = 0; i < argc; i++) {
            *(s_callbacks + i) = va_arg(args, BatterySaverUpdated);
        }
    }
    s_bat_mode = app_config()->bat_mode;
    s_st_start = app_config()->bat_mode_st_start;
    s_st_end = app_config()->bat_mode_st_end;
    s_active = is_active();
    if (s_bat_mode & BM_ST) {
        int reschedule_ms = future_ms();
        s_st_handle = app_timer_register(reschedule_ms, st_handler, NULL);
    }
    if (s_bat_mode & BM_QT) {
        s_handle = app_timer_register(POLLING_MS, handler, NULL);
    }
}

void battery_saver_deinit() {
    s_callbacks_size = 0;
    if (s_callbacks) {
        free(s_callbacks);
        s_callbacks = NULL;
    }
    if (s_handle) {
        app_timer_cancel(s_handle);
        s_handle = NULL;
    }
    if (s_st_handle) {
        app_timer_cancel(s_st_handle);
        s_st_handle = NULL;
    }
}

bool battery_saver_is_active() { 
    if (s_handle) {
        app_timer_cancel(s_handle);
        s_handle = NULL;
        handler((void *)1);
    } else {
        battery_saver_update(false);
    }
    return s_active;
}
