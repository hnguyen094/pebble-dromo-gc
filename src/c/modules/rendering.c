#include "rendering.h"
#include "config_data.h"
#include "device_data.h"
#include "weather.h"
#include "tick.h"

// Sad. It turns out that a dirty layer will redraw EVERY SINGLE LAYER IN THE WINDOW.
// For a watchface, it means EVERYTHING is redrawn even if there is only 1 layer that updates.

#define VALID(units, bit) (units & (bit | (bit-1)))
#define IS_24H_STYLE ((s_hour_mode == 0 && clock_is_24h_style()) || s_hour_mode == 1)

#define CONTAINER_PAD 40
#define TIMEBOX_CORNER 10       // Maximum should be 8  
#define TIMEBOX_OUTLINE 2
#define TIMEBOX_PAD (4 + TIMEBOX_OUTLINE)
#define TIMEBOX_W (106 + TIMEBOX_PAD + TIMEBOX_PAD)
#define TIMEBOX_H (63 + TIMEBOX_PAD + TIMEBOX_PAD) 
#define DAYDATE_FONT_SZ 19
#define DAY_W 26    // (11+2)*2
#define VISUAL_DAYDATE_W 85        // (11+2)*6+(5+2) // assuming 2-char days
#define DAY_V_PAD 1
#define AMPM_PAD 3
#define AMPM_FONT_SZ 8 
#define HHMM_FONT_SZ 32
#define HHMM_WIDTH 78           // (16+2)*4+(4+2)
#define SS_PAD_HOFFSET 2
#define SS_PAD_VOFFSET 1
#define SS_FONT_SZ 24
#define SS_WIDTH 28     // (12+2)*2
#define BRANDING_FONT_SZ 10
#define BRANDING_PAD 3
#define LOGO_SZ 25
#define DCM_SZ 7
#define DC_PAD 1
#define DCM_PAD 2
#define BAT_W 16
#define BAT_H 5
#define BAT_BODY 12
#define BAT_CORNER 1
#define BAT_PAD 0
#define BAT_TEXT_W 10
#define AUX_DATA_W 57
#define AUX_DATA_PAD 5
#define SCREEN_CORNER 3 // pixels aren't rendered in the corners

static Window *s_window;

static Layer *s_container_layer;
static Layer *s_timebox_layer;
static Layer *s_opt_aux_layer;
static Layer *s_aux_data_layer;

static BitmapLayer *s_logo_layer;
static Layer *s_mute_layer;
static Layer *s_disconnect_layer;
static Layer *s_bat_layer;

static GBitmap *s_logo_bitmap;
static GBitmap *s_disconnect_bitmap;
static GBitmap *s_mute_bitmap;

static char s_hhmm_buffer[6];       // HH:MM\0
static char s_ss_buffer[3];         // SS\0
static char s_ampm_buffer[3];       // XM\0
static char s_date_buffer[6];       // MM-DD\0
static char s_day_buffer[4];        // aaa\0
static char s_aux_data_buffer[17];  // -70'F 100% 99MPH\0
static char s_bat_buffer[5];        // 100%/0

static char s_temp_buffer[11];      // -XX^<-XX^X\0
static char s_activity_buffer[19];  // restfully sleeping\0
static char s_steps_buffer[14];     // STEPS: XXXXXX\0
static char s_hr_buffer[8];     // XXX BPM\0
static char s_precip_buffer[12];     // XXkm/h 100%\0
static char s_condition_buffer[30]; // light thunderstorm with hail\0
static char s_tz_buffer[7];         // GMT+07\0

const char *s_branding_buffer = "GROUP C SPORT CHRONO";

static TextLayer *s_hhmm_layer;
static TextLayer *s_ss_layer;
static TextLayer *s_ampm_layer;
static TextLayer *s_day_layer;
static TextLayer *s_date_layer;
static TextLayer *s_branding_layer;
static TextLayer *s_tz_layer;

static GFont s_hhmm_font;
static GFont s_ss_font;
static GFont s_ampm_font;
static GFont s_daydate_font;
static GFont s_branding_font; 
static GFont s_bat_font;

static struct DeviceData s_current_device_data;
static GColor s_accent_color;
static int s_weather_mode;
static int s_health_mode;
static int s_hour_mode;
static int s_tz_mode;
static bool s_awake;
static bool s_inverted_timebox;
static bool s_battery_percentage;

static void render_time(struct tm *tick_time, TimeUnits units_changed, TimeUnits current_units) {
    time_t current = mktime(tick_time);
    struct tm *local_time = localtime(&current);
    
    if (s_awake && strlen(app_config()->tz_code) != 0) {
        current = current + app_config()->tz_offset;
        local_time = gmtime(&current);
        snprintf(s_tz_buffer, sizeof(s_tz_buffer), "%s", app_config()->tz_code);
    } else {
        strftime(s_tz_buffer, sizeof(s_tz_buffer), "%Z", local_time);
    }

    static char fmt[6];
    // Note: no year display
    if (units_changed & (MONTH_UNIT | DAY_UNIT)) { 
        snprintf(fmt, sizeof(fmt), "%s-%s", 
                VALID(current_units, MONTH_UNIT) ? "%m" : "__", // when modifying DAYDATE_FONT, must use _ to get the full width.
                VALID(current_units, DAY_UNIT) ? "%d" : "__"
                );
        strftime(s_day_buffer, sizeof(s_day_buffer), (current_units & (DAY_UNIT | (DAY_UNIT-1))) ? "%a" : "__", local_time);
        s_day_buffer[2] = '\0';
        strftime(s_date_buffer, sizeof(s_date_buffer), fmt, local_time);
        if (s_date_buffer[0] == '0') s_date_buffer[0] = ' '; // remove month leading zero
        if (s_date_buffer[3] == '0') s_date_buffer[3] = ' '; // remove date leading zero
        layer_mark_dirty((Layer *)s_date_layer);
        layer_mark_dirty((Layer *)s_day_layer);
    }
    if (units_changed & (HOUR_UNIT | MINUTE_UNIT)) {
        snprintf(fmt, sizeof(fmt),"%s:%s",
                VALID(current_units, HOUR_UNIT) ? IS_24H_STYLE ? "%H" : "%I" : "--",
                VALID(current_units, MINUTE_UNIT) ? "%M" : "--"
                );
        strftime(s_hhmm_buffer, sizeof(s_hhmm_buffer), fmt, local_time);
        if (s_hhmm_buffer[0] == '0') s_hhmm_buffer[0] = ' ';
        layer_mark_dirty((Layer *)s_hhmm_layer);
    }
    if (units_changed & HOUR_UNIT) {
        bool is_pm = local_time->tm_hour > 11;
        bool no_show = (IS_24H_STYLE || !VALID(current_units, HOUR_UNIT) || (s_hour_mode == 2 && !is_pm));
        char *updated_ampm = no_show ? "" : is_pm ? "PM" : "AM";
        // APP_LOG(APP_LOG_LEVEL_INFO, "%d, %d, %d", is_pm, IS_24H_STYLE, s_hour_mode);
        if (strcmp(s_ampm_buffer, updated_ampm)) {
            strcpy(s_ampm_buffer, updated_ampm);
            layer_mark_dirty((Layer *)s_ampm_layer);
        }
    }
    if (units_changed & SECOND_UNIT) {
        strftime(s_ss_buffer, sizeof(s_ss_buffer), VALID(current_units, SECOND_UNIT) ? "%S" : "--", local_time);
        layer_mark_dirty((Layer *)s_ss_layer);
    }
}

static void timebox_set_text_color() {
    GColor color = s_inverted_timebox ? GColorWhite: GColorBlack; 

    text_layer_set_text_color(s_hhmm_layer, color);
    text_layer_set_text_color(s_ss_layer, color);
    text_layer_set_text_color(s_day_layer, color);
    text_layer_set_text_color(s_date_layer, color);
    text_layer_set_text_color(s_ampm_layer, color);
    text_layer_set_text_color(s_tz_layer, color);

    color = s_inverted_timebox ? GColorBlack : GColorWhite;
    text_layer_set_background_color(s_hhmm_layer, color);
    text_layer_set_background_color(s_ss_layer, color);
    text_layer_set_background_color(s_date_layer, color);
    text_layer_set_background_color(s_day_layer, color);
    text_layer_set_background_color(s_ampm_layer, color);
    text_layer_set_background_color(s_tz_layer, color);
}

void rendering_handle_device_data_update() {
    const struct DeviceData *data = device_data();
    layer_set_hidden(s_disconnect_layer, data->app_connection);
    layer_set_hidden(s_mute_layer, !data->quiet_time);
    if (
            s_current_device_data.battery.is_charging != data->battery.is_charging ||
            s_current_device_data.battery.is_plugged != data->battery.is_plugged ||
            s_current_device_data.battery.charge_percent != data->battery.charge_percent
       ) {
        snprintf(s_bat_buffer, sizeof(s_bat_buffer), "%d", data->battery.charge_percent);
        if (s_battery_percentage) layer_mark_dirty(s_bat_layer);
    } 
    if (
            s_current_device_data.activities != data->activities || 
            s_current_device_data.steps_today != data->steps_today ||
            s_current_device_data.heart_rate != data->heart_rate
       ) {
        HealthActivityMask act = data->activities;
        snprintf(s_activity_buffer, sizeof(s_activity_buffer), "%s", 
                act & HealthActivitySleep ? "sleeping" :
                act & HealthActivityRestfulSleep ? "restfully sleeping" :
                act & HealthActivityWalk ? "walking" :
                act & HealthActivityRun ? "running" :
                act ? "working out" : "no activity"
                );
        if (data->heart_rate) {
            snprintf(s_hr_buffer, sizeof(s_hr_buffer), "%d BPM", (int)data->heart_rate);
        } else {
            s_hr_buffer[0] = '\0';
        }
        snprintf(s_steps_buffer, sizeof(s_steps_buffer), "Steps: %d", (int)data->steps_today); 
        layer_mark_dirty(s_opt_aux_layer);
    }
    s_current_device_data = *data;
}

void rendering_handle_weather_update() {
    const struct WeatherData *data = weather_data();
    int wind_unit = app_config()->weather_wind_unit;
    char *wind_unit_text = 
        wind_unit == 0 ? "km/h" : 
        wind_unit == 1 ? "m/s" : 
        wind_unit == 2 ? "mph" : 
        wind_unit == 3 ? "kn" : "";
    // char *precip_unit_text = app_config()->weather_precip_unit ? "in" : "mm";
    char *temp_unit_text = app_config()->weather_temp_unit ? "F" : "C";

    snprintf(s_aux_data_buffer, sizeof(s_aux_data_buffer), "%d°%s %d%% %d%s",
            data->current_temp,
            temp_unit_text,
            data->current_precip,
            data->current_wind,
            wind_unit_text
            );
    snprintf(s_precip_buffer, sizeof(s_precip_buffer), "%d%% %d%s", 
            data->future_precip,
            data->future_wind,
            wind_unit_text
            );
    snprintf(s_condition_buffer, sizeof(s_condition_buffer), "%s", data->future_cond);
    snprintf(s_temp_buffer, sizeof(s_temp_buffer), "%d°<%d°%s", 
            data->future_min, 
            data->future_max, 
            temp_unit_text
            );
    if (app_config()->subscribe_to_data) {
        layer_mark_dirty(s_aux_data_layer);
        layer_mark_dirty(s_opt_aux_layer);
    }
}

void rendering_handle_wakeup(bool awake) {
    if (s_awake != awake) {
        s_awake = awake;
        layer_mark_dirty(s_aux_data_layer); 
        layer_mark_dirty(s_opt_aux_layer);
        
        layer_set_hidden((Layer *)s_tz_layer, !(s_tz_mode == 2 || (s_tz_mode == 1 && s_awake)));
    }
}

void rendering_handle_config_update() {
    const struct AppConfig *data = app_config();

    layer_set_hidden(s_aux_data_layer, !data->weather_mode);
    layer_set_hidden(s_opt_aux_layer, !(data->weather_mode || data->health_mode));
    
    if (s_health_mode != data->health_mode) {
        s_health_mode = data->health_mode;
        layer_mark_dirty(s_opt_aux_layer);
    }

    if (s_weather_mode != data->weather_mode) {
        s_weather_mode = data->weather_mode;
        layer_mark_dirty(s_opt_aux_layer);
        layer_mark_dirty(s_aux_data_layer);
    }

    if (s_inverted_timebox != data->inverted_timebox) {
        s_inverted_timebox = data->inverted_timebox;
        timebox_set_text_color(); 
    }
    if (s_battery_percentage != data->battery_percentage) {
        s_battery_percentage = data->battery_percentage;
        layer_mark_dirty(s_bat_layer);
    }
    if (!gcolor_equal(s_accent_color, data->accent_color)) {
        s_accent_color = data->accent_color;
        // window_set_background_color(s_window, s_accent_color);
        layer_mark_dirty(s_container_layer);
    }
    if (s_hour_mode != data->hour_mode) { // TODO: we need to trigger rendering_tick_handler with all correct args.
        s_hour_mode = data->hour_mode;
        force_tick_now(HOUR_UNIT);
    }
    if (s_tz_mode != data->tz_mode) {
        s_tz_mode = data->tz_mode;
        layer_set_hidden((Layer *)s_tz_layer, !(s_tz_mode == 2 || (s_tz_mode == 1 && s_awake)));
    }
}

void rendering_tick_handler(struct tm *tick_time, TimeUnits units_changed, TimeUnits current_units) {
    render_time(tick_time, units_changed, current_units);
    rendering_handle_weather_update();
    rendering_handle_device_data_update();
}

static void container_layer_proc(struct Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GRect unobstructed = layer_get_unobstructed_bounds(layer);
    graphics_context_set_stroke_color(ctx, s_accent_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_antialiased(ctx, false);
    graphics_context_set_fill_color(ctx, GColorBlack);
    bool squeezed = !grect_equal(&bounds, &unobstructed);
    GRect outline = squeezed 
        ? unobstructed 
        : GRect(-1, 0, bounds.size.w+2, bounds.size.h);
    graphics_fill_rect(ctx, outline, TIMEBOX_CORNER, GCornersAll);
    // if (!squeezed || ((!s_health_mode || (s_health_mode == 1 && !s_awake)) && (!s_weather_mode || (s_weather_mode == 1 && !s_awake))))
    // if (!squeezed)
        graphics_draw_round_rect(ctx, outline, TIMEBOX_CORNER);
}

static void timebox_layer_proc(struct Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GColor bg_color = s_inverted_timebox ? GColorBlack : GColorWhite;
    graphics_context_set_fill_color(ctx, bg_color);
    graphics_context_set_stroke_color(ctx, s_accent_color);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_antialiased(ctx, false);
    graphics_draw_round_rect(ctx, bounds, TIMEBOX_CORNER + TIMEBOX_OUTLINE);
    graphics_fill_rect(ctx, GRect(TIMEBOX_OUTLINE, TIMEBOX_OUTLINE, bounds.size.w - TIMEBOX_OUTLINE * 2, bounds.size.h - TIMEBOX_OUTLINE * 2), TIMEBOX_CORNER, GCornersAll);
}

static void aux_data_layer_proc(struct Layer *layer, GContext *ctx) {
    if (!s_weather_mode || (s_weather_mode == 1 && !s_awake)) return;

    GRect bounds = layer_get_bounds(layer);
    GColor color = s_inverted_timebox ? GColorWhite : GColorBlack;
    graphics_context_set_text_color(ctx, color);
    graphics_context_set_stroke_color(ctx, color);
    graphics_draw_text(ctx, s_aux_data_buffer, s_bat_font, bounds, GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    time_t due = weather_data()->timestamp + app_config()->weather_refresh_rate;
    time_t now = time(NULL);
    int width = bounds.size.w - 2 * AUX_DATA_PAD;
    int px_width = due >= now ? width * (due - now) / app_config()->weather_refresh_rate : width * (now % 2);
    graphics_draw_line(ctx, GPoint(AUX_DATA_PAD, bounds.size.h-1), GPoint(AUX_DATA_PAD + px_width, bounds.size.h-1));
    for (int i = AUX_DATA_PAD + 2 - (now % 3); i < AUX_DATA_PAD + width; i += 3) {
        graphics_draw_pixel(ctx, GPoint(i, bounds.size.h-1));
    }

    // bounds
    // graphics_draw_pixel(ctx, GPoint(AUX_DATA_PAD, bounds.size.h-1)); 
    // graphics_draw_pixel(ctx, GPoint(AUX_DATA_PAD+1, bounds.size.h-1)); 
    // graphics_draw_pixel(ctx, GPoint(AUX_DATA_PAD + width-1, bounds.size.h-1));
    // graphics_draw_pixel(ctx, GPoint(AUX_DATA_PAD + width-2, bounds.size.h-1));
}

static void opt_aux_layer_helper(GContext *ctx, GRect bounds, char *a, char *b, char *c) {
    int la = strlen(a);
    int lc = strlen(c);

    GRect a_box = GRect(bounds.origin.x, bounds.origin.y, 4 * la, BAT_H);
    GSize a_sz = graphics_text_layout_get_content_size(a, s_bat_font, a_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
    GRect c_box = GRect(bounds.origin.x + bounds.size.w - (4 * lc) + 1, bounds.origin.y, 4 * lc, BAT_H);
    GSize c_sz = graphics_text_layout_get_content_size(c, s_bat_font, c_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight);
    GRect b_box = GRect(bounds.origin.x + a_sz.w + 1, bounds.origin.y, bounds.size.w - a_sz.w - c_sz.w - 1, BAT_H);

    graphics_draw_text(ctx, a, s_bat_font, a_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, c, s_bat_font, c_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    graphics_draw_text(ctx, b, s_bat_font, b_box, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void opt_aux_layer_proc(struct Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_unobstructed_bounds(layer);
    graphics_context_set_text_color(ctx, GColorWhite);
 
    if (s_health_mode == 2 || (s_health_mode == 1 && s_awake)) {
        GRect top_bounds = GRect(SCREEN_CORNER, 0, bounds.size.w - 2 * SCREEN_CORNER, BAT_H);
        graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, BAT_H), 0, GCornerNone);
        opt_aux_layer_helper(ctx, top_bounds, s_steps_buffer, s_hr_buffer, s_activity_buffer);
    }

    if (s_weather_mode == 2 || (s_weather_mode == 1 && s_awake)) {
        GRect bottom_bounds = GRect(SCREEN_CORNER, bounds.size.h-BAT_H, bounds.size.w - 2 * SCREEN_CORNER, BAT_H);
        graphics_fill_rect(ctx, GRect(0, bounds.size.h-BAT_H, bounds.size.w, BAT_H), 0, GCornerNone);
        opt_aux_layer_helper(ctx, bottom_bounds, s_temp_buffer, s_condition_buffer, s_precip_buffer);
    }
}

static void timebox_icon_proc(struct Layer *layer, GContext *ctx, const GBitmap *bitmap) {
    GRect bounds = layer_get_bounds(layer);
    GCompOp op = s_inverted_timebox ? GCompOpAssignInverted : GCompOpAssign;
    graphics_context_set_compositing_mode(ctx, op);
    graphics_draw_bitmap_in_rect(ctx, bitmap, bounds);
}

static void mute_layer_proc(struct Layer *layer, GContext *ctx) {
    timebox_icon_proc(layer, ctx, s_mute_bitmap);
}

static void disconnect_layer_proc(struct Layer *layer, GContext *ctx) {
    timebox_icon_proc(layer, ctx, s_disconnect_bitmap);
}

static void bat_layer_proc(struct Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GColor color = s_inverted_timebox ? GColorWhite: GColorBlack;
    graphics_context_set_stroke_color(ctx, color);
    graphics_context_set_fill_color(ctx, color);
    graphics_context_set_text_color(ctx, color);
    graphics_context_set_stroke_width(ctx, 1);
    if (s_battery_percentage) {
        graphics_draw_text(ctx, s_bat_buffer, s_bat_font, GRect(0, 0, BAT_TEXT_W, BAT_H), GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
    }
    graphics_draw_round_rect(ctx, GRect(BAT_TEXT_W, 0, BAT_BODY, BAT_H), BAT_CORNER);

    int bat_percentage = (s_current_device_data.battery.charge_percent+5)*(BAT_BODY-2)/100;
    graphics_fill_rect(ctx, GRect(BAT_TEXT_W+1, bounds.origin.y+1, bat_percentage, BAT_H-2), 0, GCornerNone);

    if (s_current_device_data.battery.is_charging) { // plus sign
        graphics_fill_rect(ctx, GRect(BAT_TEXT_W + BAT_BODY+1, 1, 3, 3), 1, GCornersAll);
    } else if (s_current_device_data.battery.is_plugged) { // cable sign.
        graphics_fill_rect(ctx, GRect(BAT_TEXT_W + BAT_BODY+1, 1, 3, 3), 1, GCornersLeft);
        graphics_context_set_stroke_color(ctx, s_inverted_timebox ? GColorBlack : GColorWhite);
        graphics_draw_pixel(ctx, GPoint(BAT_TEXT_W+BAT_BODY+3, 2));
    }
}

static void layers_set_frames(GRect *full, GRect *unobstructed) {
    GRect container_frame = GRect(0, (unobstructed->size.h - TIMEBOX_H - CONTAINER_PAD) / 2, unobstructed->size.w, TIMEBOX_H + CONTAINER_PAD);
    // we define 'squeezed' to be not having sufficient padding outside the container.
    bool squeezed = !grect_equal(full, unobstructed) && (unobstructed->size.h - container_frame.size.h) < CONTAINER_PAD;
    if (squeezed) container_frame = GRect(0, 0, unobstructed->size.w, unobstructed->size.h + 1);
 
    layer_set_frame(s_container_layer, container_frame);
    int boundsl1 = container_frame.size.w - TIMEBOX_W;
    int boundsl2 = (container_frame.size.w-TIMEBOX_W)/3*2;
    GRect timebox_frame = GRect(LOGO_SZ > boundsl2 ? boundsl1 : boundsl2, (container_frame.size.h - TIMEBOX_H) / 2, TIMEBOX_W, TIMEBOX_H);
    layer_set_frame(s_timebox_layer, timebox_frame);
    layer_set_frame((Layer *)s_logo_layer, GRect((timebox_frame.origin.x-LOGO_SZ)/2, timebox_frame.origin.y, LOGO_SZ, timebox_frame.size.h));
    layer_set_frame((Layer *) s_branding_layer, GRect(BRANDING_PAD, timebox_frame.origin.y + TIMEBOX_H + BRANDING_PAD, container_frame.size.w - 2 * BRANDING_PAD, BRANDING_FONT_SZ));
    layer_set_hidden((Layer *)s_branding_layer, squeezed);

    GRect aux_frame = squeezed
        ? *unobstructed
        : GRect(0, container_frame.origin.y - BAT_H - 1, container_frame.size.w, container_frame.size.h + 2 * (BAT_H + 1)); 
    layer_set_frame(s_opt_aux_layer, aux_frame);
}

static void unobstructed_area_will_change(GRect bounds, void *context) {
    Layer *window_layer = window_get_root_layer(s_window);
    GRect full_bounds = layer_get_bounds(window_layer);
    layers_set_frames(&full_bounds, &bounds);
}

static void layers_load(Layer *window_layer) {
    GRect window_bounds = layer_get_bounds(window_layer);
    GRect unobstructed_bounds = layer_get_unobstructed_bounds(window_layer);

    s_container_layer = layer_create(GRectZero);
    s_timebox_layer = layer_create(GRectZero);
    s_logo_layer = bitmap_layer_create(GRectZero);
    s_branding_layer = text_layer_create(GRectZero);
    s_opt_aux_layer = layer_create(GRectZero);

    layers_set_frames(&window_bounds, &unobstructed_bounds);
    GRect bounds = layer_get_bounds(s_timebox_layer);


    int daydate_start = bounds.size.w - TIMEBOX_PAD - VISUAL_DAYDATE_W; 
    s_disconnect_layer = layer_create(GRect((daydate_start - DCM_SZ) / 2 + DCM_PAD, TIMEBOX_PAD, DCM_SZ, DCM_SZ));
    s_mute_layer = layer_create(GRect((daydate_start - DCM_SZ) / 2 + DCM_PAD, TIMEBOX_PAD + DAYDATE_FONT_SZ - DCM_SZ - DC_PAD, DCM_SZ, DCM_SZ));

    GRect bat_frame = GRect(bounds.size.w-TIMEBOX_PAD-BAT_W-BAT_TEXT_W, TIMEBOX_PAD + DAYDATE_FONT_SZ + AMPM_PAD + (AMPM_FONT_SZ - BAT_H) / 2 + BAT_PAD, BAT_TEXT_W + BAT_W, BAT_H);
    s_bat_layer = layer_create(bat_frame);
    s_aux_data_layer = layer_create(GRect(bat_frame.origin.x-AUX_DATA_W, bat_frame.origin.y, AUX_DATA_W, BAT_H + 2));

    s_day_layer = text_layer_create(GRect(daydate_start, TIMEBOX_PAD + DAY_V_PAD, DAY_W, DAYDATE_FONT_SZ));
    s_date_layer = text_layer_create(GRect(TIMEBOX_PAD, TIMEBOX_PAD, bounds.size.w - 2 * TIMEBOX_PAD, DAYDATE_FONT_SZ));
    s_ampm_layer = text_layer_create(GRect(TIMEBOX_PAD, TIMEBOX_PAD + DAYDATE_FONT_SZ + AMPM_PAD, bounds.size.w - 2*TIMEBOX_PAD, AMPM_FONT_SZ));
    s_hhmm_layer = text_layer_create(GRect(TIMEBOX_PAD, bounds.size.h - TIMEBOX_PAD - HHMM_FONT_SZ, HHMM_WIDTH, HHMM_FONT_SZ));
    s_ss_layer = text_layer_create(GRect(bounds.size.w - SS_WIDTH - TIMEBOX_PAD + SS_PAD_HOFFSET, bounds.size.h - TIMEBOX_PAD - SS_FONT_SZ + SS_PAD_VOFFSET, SS_WIDTH, SS_FONT_SZ));
    s_tz_layer = text_layer_create(GRect(bounds.size.w - SS_WIDTH - TIMEBOX_PAD + SS_PAD_HOFFSET, bounds.size.h - TIMEBOX_PAD - HHMM_FONT_SZ + SS_PAD_VOFFSET, SS_WIDTH, BAT_H));

    text_layer_set_text(s_hhmm_layer, s_hhmm_buffer);
    text_layer_set_text(s_ss_layer, s_ss_buffer);
    text_layer_set_text(s_ampm_layer, s_ampm_buffer);
    text_layer_set_text(s_tz_layer, s_tz_buffer);
    text_layer_set_text(s_day_layer, s_day_buffer);
    text_layer_set_text(s_branding_layer, s_branding_buffer);
    text_layer_set_text(s_date_layer, s_date_buffer);

    s_logo_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DROMO_LOGO);
    s_mute_bitmap = gbitmap_create_with_resource(RESOURCE_ID_MUTED);
    s_disconnect_bitmap = gbitmap_create_with_resource(RESOURCE_ID_DISCONNECTED);

    s_hhmm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_HHMM_32));
    s_ss_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_SS_24));
    s_ampm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_AMPM_8));
    s_daydate_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_DAYDATE_19));
    s_branding_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_BRANDING_10));
    s_bat_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_DROMO_BAT_5));

    bitmap_layer_set_bitmap(s_logo_layer, s_logo_bitmap);

    text_layer_set_font(s_hhmm_layer, s_hhmm_font);
    text_layer_set_font(s_ss_layer, s_ss_font);
    text_layer_set_font(s_ampm_layer, s_ampm_font);
    text_layer_set_font(s_date_layer, s_daydate_font);
    text_layer_set_font(s_day_layer, s_daydate_font);
    text_layer_set_font(s_branding_layer, s_branding_font);
    text_layer_set_font(s_tz_layer, s_bat_font);

    layer_set_hidden(s_aux_data_layer, !s_weather_mode);
    layer_set_hidden(s_opt_aux_layer, !(s_weather_mode || s_health_mode));
    layer_set_hidden((Layer *)s_tz_layer, !(s_tz_mode == 2 || (s_tz_mode == 1 && s_awake)));
    layer_set_update_proc(s_container_layer, container_layer_proc);
    layer_set_update_proc(s_timebox_layer, timebox_layer_proc);
    layer_set_update_proc(s_opt_aux_layer, opt_aux_layer_proc);
    layer_set_update_proc(s_mute_layer, mute_layer_proc);
    layer_set_update_proc(s_disconnect_layer, disconnect_layer_proc);
    layer_set_update_proc(s_bat_layer, bat_layer_proc);
    layer_set_update_proc(s_aux_data_layer, aux_data_layer_proc);
    layer_set_clips(s_container_layer, true);
    bitmap_layer_set_compositing_mode(s_logo_layer, GCompOpAssignInverted);
    text_layer_set_text_alignment(s_date_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_day_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_hhmm_layer, GTextAlignmentRight);
    text_layer_set_text_alignment(s_branding_layer, GTextAlignmentCenter);
    text_layer_set_text_alignment(s_tz_layer, GTextAlignmentCenter);
    text_layer_set_background_color(s_branding_layer, GColorBlack);
    text_layer_set_text_color(s_branding_layer, GColorWhite);
    timebox_set_text_color();
}

static void layers_unload() {
    layer_destroy(s_container_layer);
    layer_destroy(s_timebox_layer);
    layer_destroy(s_opt_aux_layer);
    layer_destroy(s_mute_layer);
    layer_destroy(s_disconnect_layer);
    layer_destroy(s_bat_layer);
    layer_destroy(s_aux_data_layer);

    bitmap_layer_destroy(s_logo_layer);

    gbitmap_destroy(s_logo_bitmap);
    gbitmap_destroy(s_mute_bitmap);
    gbitmap_destroy(s_disconnect_bitmap);

    text_layer_destroy(s_hhmm_layer);
    text_layer_destroy(s_ss_layer);
    text_layer_destroy(s_ampm_layer);
    text_layer_destroy(s_date_layer);
    text_layer_destroy(s_day_layer);
    text_layer_destroy(s_branding_layer);
    text_layer_destroy(s_tz_layer);

    fonts_unload_custom_font(s_hhmm_font);
    fonts_unload_custom_font(s_ss_font);
    fonts_unload_custom_font(s_ampm_font);
    fonts_unload_custom_font(s_daydate_font);
    fonts_unload_custom_font(s_branding_font);
    fonts_unload_custom_font(s_bat_font);
}

static void main_window_load(Window *window) {
    window_set_background_color(window, GColorBlack);
    s_inverted_timebox = app_config()->inverted_timebox;
    s_health_mode = app_config()->health_mode;
    s_weather_mode = app_config()->weather_mode;
    s_battery_percentage = app_config()->battery_percentage;
    s_accent_color = app_config()->accent_color;
    s_hour_mode = app_config()->hour_mode;
    s_tz_mode = app_config()->tz_mode;

    Layer *window_layer = window_get_root_layer(window);
    layers_load(window_layer);

    layer_add_child(window_layer, s_container_layer);
    layer_add_child(window_layer, s_opt_aux_layer);
    layer_add_child(s_container_layer, s_timebox_layer);
    layer_add_child(s_container_layer, (Layer *)s_logo_layer);
    layer_add_child(s_container_layer, (Layer *)s_branding_layer);
    layer_add_child(s_timebox_layer, (Layer *)s_hhmm_layer); 
    layer_add_child(s_timebox_layer, (Layer *)s_ss_layer); 
    layer_add_child(s_timebox_layer, (Layer *)s_ampm_layer); 
    layer_add_child(s_timebox_layer, (Layer *)s_date_layer); 
    layer_add_child(s_timebox_layer, (Layer *)s_day_layer); 
    layer_add_child(s_timebox_layer, (Layer *)s_tz_layer); 
    layer_add_child(s_timebox_layer, s_mute_layer); 
    layer_add_child(s_timebox_layer, s_disconnect_layer); 
    layer_add_child(s_timebox_layer, s_bat_layer); 
    layer_add_child(s_timebox_layer, s_aux_data_layer);

    UnobstructedAreaHandlers handlers = {
        .will_change = unobstructed_area_will_change
    };
    unobstructed_area_service_subscribe(handlers, NULL);
}

static void main_window_unload(Window *window) {
    unobstructed_area_service_unsubscribe();
    layers_unload();
    window_destroy(s_window);
}

void main_window_create_register() {
    if (!s_window) {
        s_window = window_create();
        window_set_window_handlers(s_window, (WindowHandlers) {
                .load = main_window_load,
                .unload = main_window_unload,
                });
    }
    window_stack_push(s_window, true);
}
