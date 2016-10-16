#include <pebble.h>
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/fpath.h>
#include <pebble-fctx/ffont.h>

// battery icon size:
#define BAT_W 10
#define BAT_H 15

// message buffer size:
#define MESSAGE_BUF 128

// TODO Use `layer_get_frame` instead of hardcoding sizes in callbacks!
// TODO Add `const` where appropriate!

#define max(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a > _b ? _a : _b; })
#define min(a,b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
       _a < _b ? _a : _b; })

// --------------------------------------------------------------------------
// Types and global variables.
// --------------------------------------------------------------------------

static Window* g_window;
static Layer* g_layer;                        // Main layer updated every minute - clock and health data.
static Layer* g_battery_layer;                // Layer updated on battery events.
static Layer* g_connection_layer;             // Layer updated on connection events.
static TextLayer* g_health_cals_text_layer;   // Layer updated on health events.
static TextLayer* g_health_meters_text_layer; // Layer updated on health events.
static TextLayer* g_health_sleep_text_layer;  // Layer updated on health events.
static TextLayer* g_health_bpm_text_layer;    // Layer updated on heart beat events.
static Layer* g_health_bpm_graph_layer;       // Layer updated on heart beat events or on minute ticks.
static Layer* g_weather_temp_layer;           // Layer updated on weather events from PebbleKit messages.
static Layer* g_weather_icon_layer;           // Layer updated on weather events from PebbleKit messages.
static Layer* g_weather_precipprob_layer;     // Layer updated on weather events from PebbleKit messages.
static Layer* g_weather_precipgraph_layer;    // Layer updated on weather events from PebbleKit messages or on minute ticks.
static struct tm g_local_time;
static uint8_t g_battery_level;
static int8_t g_connected; // TODO Should be bool!
static int8_t g_temp;
static int8_t g_tempmax;
static int8_t g_tempmin;
static uint8_t g_precipprob;
static uint8_t g_weather_icon; // TODO Use less obfuscated data type!
static uint8_t g_weather_precip_array[60];
static uint8_t g_ticks_since_weather_array_update;
static AppSync g_sync;
static uint8_t g_sync_buffer[MESSAGE_BUF];

enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,
  WEATHER_TEMPERATURE_KEY = 0x1,
  WEATHER_TEMPERATUREMAX_KEY = 0x2,
  WEATHER_TEMPERATUREMIN_KEY = 0x3,
  WEATHER_PRECIP_PROB_KEY = 0x4,
  WEATHER_PRECIP_ARRAY_KEY = 0x5,
};

// --------------------------------------------------------------------------
// Utility functions.
// --------------------------------------------------------------------------

static inline FPoint clockToCartesian(FPoint center, fixed_t w, fixed_t h, int32_t angle) {
    FPoint pt;
    int32_t c = cos_lookup(angle);
    int32_t s = sin_lookup(angle);
    pt.x = center.x + s * w / TRIG_MAX_RATIO;
    pt.y = center.y - c * h / TRIG_MAX_RATIO;
    return pt;
}

// --------------------------------------------------------------------------
// The main drawing function.
// --------------------------------------------------------------------------

static void on_layer_update(Layer* layer, GContext* ctx) {
    GRect bounds = layer_get_unobstructed_bounds(layer);
    int16_t w = bounds.size.w;
    int16_t h = bounds.size.h;
    FPoint center = FPointI(w / 2, h / 2);
    fixed_t f_w = INT_TO_FIXED(w);
    fixed_t f_h = INT_TO_FIXED(h);
    int32_t minute_angle = g_local_time.tm_sec * TRIG_MAX_ANGLE / 60 + TRIG_MAX_ANGLE/2;
    int32_t hour_angle   = (g_local_time.tm_hour % 12) * TRIG_MAX_ANGLE / 12
                         +  g_local_time.tm_min        * TRIG_MAX_ANGLE / (12 * 60)
                         +  TRIG_MAX_ANGLE/2;

    FContext fctx;
    fctx_init_context(&fctx, ctx);
    fctx_set_color_bias(&fctx, 0);
    
    // Draw the pips.
    fctx_set_fill_color(&fctx, GColorWhite);
    fctx_begin_fill(&fctx);
    for (int m = 0; m < 60; ++m) {
        int32_t angle = m * TRIG_MAX_ANGLE / 60;
        FPoint p = clockToCartesian(center, f_w/3, f_h/3, angle);
        fctx_set_offset(&fctx, p);
        fctx_set_rotation(&fctx, angle);
        if (0 == m % 15) {
            fctx_move_to(&fctx, FPointI( 0,   2));
            fctx_line_to(&fctx, FPointI( 3, -10));
            fctx_line_to(&fctx, FPointI(-3, -10));
            fctx_close_path(&fctx);
        } else if (0 == m % 5) {
            fctx_move_to(&fctx, FPointI( 0,  0));
            fctx_line_to(&fctx, FPointI( 2, -6));
            fctx_line_to(&fctx, FPointI(-2, -6));
            fctx_close_path(&fctx);
        } else {
            fctx_move_to(&fctx, FPointI(0,  0));
            fctx_line_to(&fctx, FPointI(0, -3));
            fctx_line_to(&fctx, FPointI(1, -3));
            fctx_line_to(&fctx, FPointI(1,  0));
            fctx_close_path(&fctx);
        }
    }
    fctx_end_fill(&fctx);
    
    // Draw the minute hand.
    fctx_begin_fill(&fctx);
    fctx_set_offset(&fctx, FPoint(0,0));
    fctx_set_scale(&fctx, FPoint(f_h, f_h), FPoint(f_h, f_h));
    fctx_set_rotation(&fctx, 0);
    FPoint minute_p = clockToCartesian(center, f_w/3, f_h/3, minute_angle);
    FPoint minute_p1 = clockToCartesian(center, f_w*8/30, f_h*8/30, minute_angle+TRIG_MAX_ANGLE/60);
    FPoint minute_p2 = clockToCartesian(center, f_w*8/30, f_h*8/30, minute_angle-TRIG_MAX_ANGLE/60);
    fctx_set_fill_color(&fctx, GColorDarkGray);
    fctx_move_to(&fctx, minute_p);
    fctx_line_to(&fctx, minute_p1);
    fctx_line_to(&fctx, minute_p2);
    fctx_close_path(&fctx);    
    fctx_end_fill(&fctx);

    // Draw the hour hand.
    fctx_set_scale(&fctx, FPoint(f_h, f_h), FPoint(f_w*13/50, f_h*13/50));
    fctx_begin_fill(&fctx);
    fctx_set_offset(&fctx, center);
    fctx_set_fill_color(&fctx, GColorBlack);
    fctx_set_rotation(&fctx, hour_angle);
    fctx_move_to(&fctx, FPointI(  0, -15));
    fctx_line_to(&fctx, FPointI( 15,   0));
    fctx_line_to(&fctx, FPoint (  0, f_h));
    fctx_line_to(&fctx, FPointI(-15,   0));
    fctx_close_path(&fctx);
    fctx_end_fill(&fctx);
    fctx_set_scale(&fctx, FPoint(f_h, f_h), FPoint(f_w/5, f_h/5));
    fctx_begin_fill(&fctx);
    fctx_set_fill_color(&fctx, GColorWhite);
    fctx_set_rotation(&fctx, hour_angle);
    fctx_move_to(&fctx, FPointI(  0, -15));
    fctx_line_to(&fctx, FPointI( 15,   0));
    fctx_line_to(&fctx, FPoint (  0, f_h));
    fctx_line_to(&fctx, FPointI(-15,   0));
    fctx_close_path(&fctx);
    fctx_end_fill(&fctx);
  
    // Draw the center.
    fctx_begin_fill(&fctx);
    fctx_set_scale(&fctx, FPoint(f_w, f_h), FPoint(f_w, f_h));
    fctx_set_fill_color(&fctx, GColorBlack);
    fctx_plot_circle(&fctx, &center, INT_TO_FIXED(1));
    fctx_end_fill(&fctx);

    fctx_deinit_context(&fctx);

    // Draw the date.
    char date_string[7];
    strftime(date_string, sizeof date_string, "%b %d", &g_local_time);
    graphics_draw_text(ctx, date_string,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(w/3,h*2/3,w/3,15), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void on_battery_layer_update(Layer* layer, GContext* ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, GRect(0, 2, BAT_W, BAT_H));
    graphics_draw_rect(ctx, GRect(BAT_W/2-2, 0, 4, 2));
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_rect(ctx, GRect(BAT_W/2-1, 1, 2, 2));
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(2, 4+(BAT_H-4)*(100-g_battery_level)/100, BAT_W-4, (BAT_H-4)*g_battery_level/100), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(2, BAT_H-1, BAT_W-4, 1), 0, GCornerNone); // XXX Due to rounding errors.
    if (g_battery_level<=5) {
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_rect(ctx, GRect(2, BAT_H-1, BAT_W-4, 1), 0, GCornerNone);
    }
}

static void on_connection_layer_update(Layer* layer, GContext* ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    if (g_connected != 0) {
        graphics_draw_line(ctx, GPoint(3, 0), GPoint(3, 12));
        graphics_draw_line(ctx, GPoint(3, 0), GPoint(6, 3));
        graphics_draw_line(ctx, GPoint(3, 12), GPoint(6, 9));
    }
    graphics_draw_line(ctx, GPoint(0, 3), GPoint(6, 9));
    graphics_draw_line(ctx, GPoint(0, 9), GPoint(6, 3));
}

static void on_health_bpm_graph_layer_update(Layer* layer, GContext* ctx) {
    HealthMinuteData minute_data[30];
    time_t t1, t2;
    // TODO Why not health_service_get_minute_history(minute_data, sizeof(minute_data), &t1, &t2));
    health_service_get_minute_history(&minute_data[0], 30, &t1, &t2);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(1, 11, 33, 1), 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, GColorWhite);
    for (int i=0; i<30; i++) {
        if (minute_data[29-i].is_invalid) {continue;}
        int y = min(20, max(1, 20-(minute_data[29-i].heart_rate_bpm-50)*20/100));
        graphics_draw_line(ctx, GPoint(i+2, y), GPoint(i+2, 20));
    }
    graphics_draw_rect(ctx, GRect(0,0,34,22));
    graphics_fill_rect(ctx, GRect(16, 1, 1, 20), 0, GCornerNone);
}

static void on_weather_temp_layer_update(Layer* layer, GContext* ctx) {
    char temp_string[4];
    snprintf(temp_string, sizeof temp_string, "%d", g_temp);
    graphics_draw_text(ctx, temp_string,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(0,7,20,15), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    snprintf(temp_string, sizeof temp_string, "%d", g_tempmax);
    graphics_draw_text(ctx, temp_string,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(20-((g_tempmax<0)?5:0),0,20,15), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    snprintf(temp_string, sizeof temp_string, "%d", g_tempmin);
    graphics_draw_text(ctx, temp_string,
                       fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(20-((g_tempmin<0)?5:0),14,20,15), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
}

static void on_weather_icon_layer_update(Layer* layer, GContext* ctx) {
    static GBitmap *s_bitmap;
    if (s_bitmap) {
      gbitmap_destroy(s_bitmap);
    }
    uint32_t resource_id;
    if (g_weather_icon) {
        switch (g_weather_icon) { // TODO Mark in readme https://icons8.com/ as icons' source!
            case  1: resource_id = RESOURCE_ID_Sun_25;
            case  2: resource_id = RESOURCE_ID_Bright_Moon_25;
            case  3: resource_id = RESOURCE_ID_Rain_25;
            case  4: resource_id = RESOURCE_ID_Snow_25;
            case  5: resource_id = RESOURCE_ID_Sleet_25;
            case  6: resource_id = RESOURCE_ID_Air_Element_25;
            case  7: resource_id = RESOURCE_ID_Dust_25;
            case  8: resource_id = RESOURCE_ID_Clouds_25;
            case  9: resource_id = RESOURCE_ID_Partly_Cloudy_Day_25;
            case 10: resource_id = RESOURCE_ID_Partly_Cloudy_Night_25;
        }
        s_bitmap  = gbitmap_create_with_resource(resource_id);
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        graphics_draw_bitmap_in_rect(ctx, s_bitmap, GRect(0,0,25,25));
    } else {
        graphics_draw_text(ctx, "?",
                           fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                           GRect(0,0,25,25), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);

    }
}

static void on_weather_precipprob_layer_update(Layer* layer, GContext* ctx) {
    char percent_string[4];
    if (g_precipprob > 0) {
        snprintf(percent_string, sizeof percent_string, "%d", g_precipprob);
        graphics_draw_text(ctx, percent_string,
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(0,2,20,15), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
        graphics_draw_text(ctx, "%",
                           fonts_get_system_font(FONT_KEY_GOTHIC_14),
                           GRect(0,13,20,15), GTextOverflowModeWordWrap, GTextAlignmentRight, NULL);
    }
}

static void on_weather_precipgraph_layer_update(Layer* layer, GContext* ctx) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    int count = 0;
    int i;
    for (i=0; i<45; i++) {
        int i_off = g_ticks_since_weather_array_update+i;      
        if (i_off < 60) {
            if (g_weather_precip_array[i_off] > 0) {
                count += 1;
                graphics_draw_line(ctx, GPoint(i+2, 25-g_weather_precip_array[i_off]/10), GPoint(i+2, 25));
            }
        } else {
            break;
        }
    }
    if (count > 0) {
        graphics_draw_rect(ctx, GRect(0,0,49,27));
        graphics_context_set_fill_color(ctx, GColorDarkGray);
        graphics_fill_rect(ctx, GRect(17, 1, 1, 25), 0, GCornerNone);
        graphics_fill_rect(ctx, GRect(32, 1, 1, 25), 0, GCornerNone);
        if (g_ticks_since_weather_array_update>15) {
            graphics_fill_rect(ctx, GRect(i+1, 23, 46-i, 2), 0, GCornerNone);
        }
    }
}

// --------------------------------------------------------------------------
// System event handlers.
// --------------------------------------------------------------------------

static void on_tick_timer(struct tm* tick_time, TimeUnits units_changed) {
    g_local_time = *tick_time;
    g_ticks_since_weather_array_update += 1;
    layer_mark_dirty(g_layer);
    layer_mark_dirty(g_health_bpm_graph_layer);
    layer_mark_dirty(g_weather_precipgraph_layer);
}

static void on_battery_state(BatteryChargeState state) {
    g_battery_level = state.charge_percent;
    layer_mark_dirty(g_battery_layer);
}

static void on_connection(bool connected) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "g con was: %d", g_connected);
    g_connected = connected ? 1 : 0;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "g con is: %d", g_connected);
    layer_mark_dirty(g_connection_layer);
}

static void on_health(const HealthEventType event, void* context) {
    static char Cal_string[13];
    static char meter_string[7];
    static char sleep_string[13];
    static char bpm_string[8];
    switch (event) {
        case HealthEventHeartRateUpdate:
            snprintf(bpm_string, sizeof bpm_string, "\U00002764%d", (int)health_service_peek_current_value(HealthMetricHeartRateBPM));
            text_layer_set_text(g_health_bpm_text_layer, bpm_string);
            break;
        default:
            snprintf(Cal_string, sizeof Cal_string, "%dCal(%d)", (int)(health_service_sum_today(HealthMetricRestingKCalories)+health_service_sum_today(HealthMetricActiveKCalories)), (int)health_service_sum_today(HealthMetricActiveKCalories)); 
            text_layer_set_text(g_health_cals_text_layer, Cal_string);
            snprintf(meter_string, sizeof meter_string, "%dm", (int)health_service_sum_today(HealthMetricWalkedDistanceMeters)); 
            text_layer_set_text(g_health_meters_text_layer, meter_string);
            int sleep = health_service_sum_today(HealthMetricSleepSeconds);
            int restful = health_service_sum_today(HealthMetricSleepRestfulSeconds);
            snprintf(sleep_string, sizeof sleep_string, "%d%%/%d.%dh", restful*100/sleep, sleep/3600, (sleep%3600)*10/3600); 
            text_layer_set_text(g_health_sleep_text_layer, sleep_string);
            break;
    }
}

static void on_sync_error(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void on_sync_tuple_change(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
    switch (key) {
        case WEATHER_ICON_KEY:
            g_weather_icon = new_tuple->value->uint8;
            layer_mark_dirty(g_weather_icon_layer);
            break;
        case WEATHER_TEMPERATURE_KEY:
            g_temp = new_tuple->value->int8;
            layer_mark_dirty(g_weather_temp_layer);
            break;
        case WEATHER_TEMPERATUREMAX_KEY:
            g_tempmax = new_tuple->value->int8;
            layer_mark_dirty(g_weather_temp_layer);
            break;
        case WEATHER_TEMPERATUREMIN_KEY:
            g_tempmin = new_tuple->value->int8;
            layer_mark_dirty(g_weather_temp_layer);
            break;
        case WEATHER_PRECIP_PROB_KEY:
            g_precipprob = new_tuple->value->uint8;
            layer_mark_dirty(g_weather_precipprob_layer);
            break;
        case WEATHER_PRECIP_ARRAY_KEY:
            for (int i=0; i<new_tuple->length; i++) {g_weather_precip_array[i] = new_tuple->value->data[i];}
            g_ticks_since_weather_array_update = 0;
            layer_mark_dirty(g_weather_precipgraph_layer);
            break;
        default:
            break;
    }
}


// --------------------------------------------------------------------------
// Initialization and teardown.
// --------------------------------------------------------------------------

static void init() {
    g_window = window_create();
    window_set_background_color(g_window, GColorBlack);
    window_stack_push(g_window, true);
    Layer* window_layer = window_get_root_layer(g_window);
    GRect window_frame = layer_get_frame(window_layer);

    g_layer = layer_create(window_frame);
    layer_set_update_proc(g_layer, &on_layer_update);
    layer_add_child(window_layer, g_layer);
    GRect bounds = layer_get_unobstructed_bounds(g_layer);

    g_battery_layer = layer_create(GRect(1, bounds.size.h/2-BAT_H/2-4, BAT_W, BAT_H+2));
    layer_set_update_proc(g_battery_layer, &on_battery_layer_update);
    layer_add_child(window_layer, g_battery_layer);

    g_connection_layer = layer_create(GRect(BAT_W/2-3, bounds.size.h/2+BAT_H/2+2, 7, 13));
    layer_set_update_proc(g_connection_layer, &on_connection_layer_update);
    layer_add_child(window_layer, g_connection_layer);

    g_health_bpm_graph_layer = layer_create(GRect(1, 1, 34, 22));
    layer_set_update_proc(g_health_bpm_graph_layer, &on_health_bpm_graph_layer_update);
    layer_add_child(window_layer, g_health_bpm_graph_layer);

    g_health_bpm_text_layer = text_layer_create(GRect(1, 28, bounds.size.w/4, 14));
    layer_add_child(window_layer, text_layer_get_layer(g_health_bpm_text_layer));
    text_layer_set_background_color(g_health_bpm_text_layer, GColorBlack);
    text_layer_set_text_color(g_health_bpm_text_layer, GColorWhite);
    text_layer_set_font(g_health_bpm_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    
    g_health_meters_text_layer = text_layer_create(GRect(1, bounds.size.h-43, bounds.size.w/4, 14));
    layer_add_child(window_layer, text_layer_get_layer(g_health_meters_text_layer));
    text_layer_set_background_color(g_health_meters_text_layer, GColorBlack);
    text_layer_set_text_color(g_health_meters_text_layer, GColorWhite);
    text_layer_set_font(g_health_meters_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    
    g_health_sleep_text_layer = text_layer_create(GRect(1, bounds.size.h-29, bounds.size.w*2/5-3, 14));
    layer_add_child(window_layer, text_layer_get_layer(g_health_sleep_text_layer));
    text_layer_set_background_color(g_health_sleep_text_layer, GColorBlack);
    text_layer_set_text_color(g_health_sleep_text_layer, GColorWhite);
    text_layer_set_font(g_health_sleep_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

    g_health_cals_text_layer = text_layer_create(GRect(1, bounds.size.h-15, bounds.size.w/2, 14));
    layer_add_child(window_layer, text_layer_get_layer(g_health_cals_text_layer));
    text_layer_set_background_color(g_health_cals_text_layer, GColorBlack);
    text_layer_set_text_color(g_health_cals_text_layer, GColorWhite);
    text_layer_set_font(g_health_cals_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));

    g_weather_temp_layer = layer_create(GRect(bounds.size.w-32, bounds.size.h/2, 32, 30));
    layer_set_update_proc(g_weather_temp_layer, &on_weather_temp_layer_update);
    layer_add_child(window_layer, g_weather_temp_layer);

    g_weather_icon_layer = layer_create(GRect(bounds.size.w-29, bounds.size.h*21/30-1, 25, 25));
    layer_set_update_proc(g_weather_icon_layer, &on_weather_icon_layer_update);
    layer_add_child(window_layer, g_weather_icon_layer);

    g_weather_precipprob_layer = layer_create(GRect(bounds.size.w-20, bounds.size.h-30, 20, 30));
    layer_set_update_proc(g_weather_precipprob_layer, &on_weather_precipprob_layer_update);
    layer_add_child(window_layer, g_weather_precipprob_layer);

    g_weather_precipgraph_layer = layer_create(GRect(bounds.size.w/2+4, bounds.size.h-27, 49, 27));
    layer_set_update_proc(g_weather_precipgraph_layer, &on_weather_precipgraph_layer_update);
    layer_add_child(window_layer, g_weather_precipgraph_layer);
    
    time_t now = time(NULL);
    g_local_time = *localtime(&now);
    tick_timer_service_subscribe(SECOND_UNIT, &on_tick_timer);
  
    battery_state_service_subscribe(&on_battery_state);
  
    health_service_events_subscribe(&on_health, NULL);

    connection_service_subscribe((ConnectionHandlers) {.pebble_app_connection_handler = on_connection});
  
    Tuplet initial_values[] = {
        TupletInteger(WEATHER_ICON_KEY, (uint8_t) 0),
        TupletInteger(WEATHER_TEMPERATURE_KEY, (int8_t) -99),
        TupletInteger(WEATHER_TEMPERATUREMAX_KEY, (int8_t)-99),
        TupletInteger(WEATHER_TEMPERATUREMIN_KEY, (int8_t)-99),
        TupletInteger(WEATHER_PRECIP_PROB_KEY, (uint8_t)100),
        TupletBytes(WEATHER_PRECIP_ARRAY_KEY, g_weather_precip_array, sizeof(g_weather_precip_array))
    };

    app_sync_init(&g_sync, g_sync_buffer, sizeof(g_sync_buffer),
                  initial_values, ARRAY_LENGTH(initial_values),
                  on_sync_tuple_change, on_sync_error, NULL);
    app_message_open(MESSAGE_BUF, MESSAGE_BUF);
}

static void deinit() {
    battery_state_service_unsubscribe();
    tick_timer_service_unsubscribe();
    connection_service_unsubscribe();
    window_destroy(g_window);
    layer_destroy(g_layer);
    layer_destroy(g_battery_layer);
    layer_destroy(g_connection_layer);
    layer_destroy(g_weather_temp_layer);
    layer_destroy(g_weather_icon_layer);
    layer_destroy(g_weather_precipprob_layer);
    layer_destroy(g_weather_precipgraph_layer);
    app_sync_deinit(&g_sync);
}

// --------------------------------------------------------------------------
// The main event loop.
// --------------------------------------------------------------------------

int main() {
    init();
    app_event_loop();
    deinit();
}
