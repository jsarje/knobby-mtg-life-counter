#include "knob.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "platform_services.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LIFE_MIN -999
#define LIFE_MAX 999
#define DEFAULT_LIFE_TOTAL 40
#define DEFAULT_BRIGHTNESS_PERCENT 25
#define INTRO_CHAR_COUNT 7
#define MULTIPLAYER_COUNT 4
#define KNOB_EVENT_QUEUE_SIZE 32
#define KNOB_EVENT_PROCESS_BUDGET 8U
#define KNOB_EVENT_BURST_BUDGET 16U
#define KNOB_EVENT_DROP_LOG_INTERVAL 8U

typedef struct
{
    knob_event_t event;
} knob_input_event_t;

typedef enum
{
    MULTIPLAYER_MENU_PLAYER,
    MULTIPLAYER_MENU_GLOBAL
} multiplayer_menu_mode_t;

static const char *KNOB_GUI_TAG = "knob_gui";

static int brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;

// Screen objects
static lv_obj_t *screen_intro = NULL;
static lv_obj_t *screen_settings = NULL;
static lv_obj_t *screen_multiplayer = NULL;
static lv_obj_t *screen_multiplayer_menu = NULL;
static lv_obj_t *screen_multiplayer_name = NULL;
static lv_obj_t *screen_multiplayer_cmd_select = NULL;
static lv_obj_t *screen_multiplayer_cmd_damage = NULL;
static lv_obj_t *screen_multiplayer_all_damage = NULL;

// Intro widgets
static lv_obj_t *intro_letters[INTRO_CHAR_COUNT];

// Settings widgets
static lv_obj_t *label_settings_title = NULL;
static lv_obj_t *label_settings_value = NULL;
static lv_obj_t *label_settings_hint = NULL;
static lv_obj_t *label_settings_battery = NULL;
static lv_obj_t *label_settings_battery_detail = NULL;
static lv_obj_t *arc_brightness = NULL;

// Multiplayer widgets
static lv_obj_t *multiplayer_quadrants[MULTIPLAYER_COUNT];
static lv_obj_t *label_multiplayer_life[MULTIPLAYER_COUNT];
static lv_obj_t *label_multiplayer_name[MULTIPLAYER_COUNT];
static lv_obj_t *label_multiplayer_menu_title = NULL;
static lv_obj_t *button_multiplayer_menu_rename = NULL;
static lv_obj_t *button_multiplayer_menu_cmd_damage = NULL;
static lv_obj_t *button_multiplayer_menu_all_damage = NULL;
static lv_obj_t *button_multiplayer_menu_menu = NULL;
static lv_obj_t *button_multiplayer_menu_reset = NULL;
static lv_obj_t *button_multiplayer_menu_back = NULL;
static lv_obj_t *label_multiplayer_name_title = NULL;
static lv_obj_t *textarea_multiplayer_name = NULL;
static lv_obj_t *keyboard_multiplayer_name = NULL;
static lv_obj_t *label_multiplayer_cmd_select_title = NULL;
static lv_obj_t *button_multiplayer_cmd_target[MULTIPLAYER_COUNT - 1];
static lv_obj_t *label_multiplayer_cmd_target[MULTIPLAYER_COUNT - 1];
static lv_obj_t *label_multiplayer_cmd_damage_title = NULL;
static lv_obj_t *label_multiplayer_cmd_damage_value = NULL;
static lv_obj_t *label_multiplayer_cmd_damage_hint = NULL;
static lv_obj_t *label_multiplayer_all_damage_title = NULL;
static lv_obj_t *label_multiplayer_all_damage_value = NULL;
static lv_obj_t *label_multiplayer_all_damage_hint = NULL;

static lv_timer_t *intro_timer = NULL;
static lv_timer_t *multiplayer_life_preview_timer = NULL;
static bool knob_initialized = false;
static knob_input_event_t knob_event_queue[KNOB_EVENT_QUEUE_SIZE];
static volatile uint8_t knob_event_head = 0;
static volatile uint8_t knob_event_tail = 0;
static volatile uint32_t knob_event_overflow_count = 0;
static volatile uint8_t knob_event_high_watermark = 0;
static portMUX_TYPE knob_event_queue_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t knob_event_overflow_reported = 0;

static int battery_percent = -1;
static float battery_voltage = 0.0f;
static uint32_t battery_sample_tick = 0;
static bool battery_sample_valid = false;
static uint8_t intro_step = 0;
static int multiplayer_life[MULTIPLAYER_COUNT] = {40, 40, 40, 40};
static int multiplayer_selected = 0;
static int multiplayer_menu_player = 0;
static int multiplayer_cmd_source = 0;
static int multiplayer_cmd_target = -1;
static int multiplayer_cmd_target_choices[MULTIPLAYER_COUNT - 1] = {-1, -1, -1};
static int multiplayer_cmd_damage_totals[MULTIPLAYER_COUNT][MULTIPLAYER_COUNT] = {{0}};
static int multiplayer_all_damage_value = 0;
static multiplayer_menu_mode_t multiplayer_menu_mode = MULTIPLAYER_MENU_PLAYER;
static bool multiplayer_swipe_tracking = false;
static int multiplayer_pending_life_delta = 0;
static int multiplayer_preview_player = -1;
static bool multiplayer_life_preview_active = false;
static lv_point_t multiplayer_swipe_start = {0, 0};
static char multiplayer_names[MULTIPLAYER_COUNT][16] = {"P1", "P2", "P3", "P4"};

// Static UI assets
static const float battery_curve_voltages[] = {3.35f, 3.55f, 3.68f, 3.74f, 3.80f, 3.88f, 3.96f, 4.06f, 4.18f};
static const int battery_curve_percentages[] = {0, 5, 12, 22, 34, 48, 64, 82, 100};
static const char *intro_text[INTRO_CHAR_COUNT] = {"k", "n", "o", "b", "b", "y", "."};
static const uint32_t intro_colors[INTRO_CHAR_COUNT] = {0x9C5CFF, 0xF6C945, 0x42A5F5, 0x43A047, 0x43A047, 0xE53935, 0xFFFFFF};
static const lv_coord_t intro_x[INTRO_CHAR_COUNT] = {56, 98, 140, 182, 214, 246, 284};

static void open_multiplayer_screen(void);

static uint8_t knob_queue_count_unsafe(void)
{
    if (knob_event_head >= knob_event_tail) {
        return (uint8_t)(knob_event_head - knob_event_tail);
    }

    return (uint8_t)(KNOB_EVENT_QUEUE_SIZE - knob_event_tail + knob_event_head);
}

static void report_knob_queue_status(void)
{
    uint32_t overflow_count;
    uint8_t high_watermark;
    uint8_t pending_count;

    taskENTER_CRITICAL(&knob_event_queue_lock);
    overflow_count = knob_event_overflow_count;
    high_watermark = knob_event_high_watermark;
    pending_count = knob_queue_count_unsafe();
    taskEXIT_CRITICAL(&knob_event_queue_lock);

    if (overflow_count == knob_event_overflow_reported) {
        return;
    }

    if ((overflow_count - knob_event_overflow_reported) < KNOB_EVENT_DROP_LOG_INTERVAL && pending_count != 0U) {
        return;
    }

    ESP_LOGW(KNOB_GUI_TAG,
             "Knob queue overflow: dropped=%lu pending=%u high_water=%u capacity=%u",
             (unsigned long)overflow_count,
             (unsigned int)pending_count,
             (unsigned int)high_watermark,
             (unsigned int)(KNOB_EVENT_QUEUE_SIZE - 1));
    knob_event_overflow_reported = overflow_count;
}

static int clamp_life(int value)
{
    if (value < LIFE_MIN) return LIFE_MIN;
    if (value > LIFE_MAX) return LIFE_MAX;
    return value;
}

static int clamp_brightness(int value)
{
    if (value < 5) return 5;
    if (value > 100) return 100;
    return value;
}

static int clamp_percent(int value)
{
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}

static int battery_percent_from_voltage(float voltage)
{
    size_t i;

    if (voltage <= battery_curve_voltages[0]) return 0;
    for (i = 1; i < (sizeof(battery_curve_voltages) / sizeof(battery_curve_voltages[0])); i++) {
        if (voltage <= battery_curve_voltages[i]) {
            float low_v = battery_curve_voltages[i - 1];
            float high_v = battery_curve_voltages[i];
            int low_p = battery_curve_percentages[i - 1];
            int high_p = battery_curve_percentages[i];
            float ratio = (voltage - low_v) / (high_v - low_v);
            return clamp_percent((int)(low_p + ((high_p - low_p) * ratio) + 0.5f));
        }
    }

    return 100;
}

static void update_battery_measurement(bool force)
{
    if (!force && battery_sample_valid && (lv_tick_elaps(battery_sample_tick) < 5000)) {
        return;
    }

    battery_voltage = knob_read_battery_voltage();
    battery_sample_tick = lv_tick_get();
    battery_sample_valid = (battery_voltage > 0.0f);
}

static int read_battery_percent(void)
{
    update_battery_measurement(false);
    if (!battery_sample_valid) return -1;
    return battery_percent_from_voltage(battery_voltage);
}

static void refresh_battery_ui(void)
{
    char buf[32];
    char detail_buf[48];

    battery_percent = read_battery_percent();
    if (label_settings_battery == NULL) return;

    if (battery_percent < 0) {
        lv_label_set_text(label_settings_battery, "Battery: --%");
        if (label_settings_battery_detail != NULL) {
            lv_label_set_text(label_settings_battery_detail, "No calibrated reading");
        }
        return;
    }

    snprintf(buf, sizeof(buf), "Battery: %d%%", battery_percent);
    lv_label_set_text(label_settings_battery, buf);
    if (label_settings_battery_detail != NULL) {
        snprintf(detail_buf, sizeof(detail_buf), "%.2fV calibrated", battery_voltage);
        lv_label_set_text(label_settings_battery_detail, detail_buf);
    }
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_center(label);

    return btn;
}

static void refresh_intro_ui(void)
{
    int i;

    for (i = 0; i < INTRO_CHAR_COUNT; i++) {
        if (intro_letters[i] == NULL) continue;

        if (i < intro_step) {
            lv_obj_clear_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static lv_color_t get_player_base_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x7B1FE0, 0x29B6F6, 0xFFD600, 0xA5D6A7};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x303030);
    return lv_color_hex(colors[index]);
}

static lv_color_t get_player_active_color(int index)
{
    static const uint32_t colors[MULTIPLAYER_COUNT] = {0x9C4DFF, 0x4FC3F7, 0xFFEA61, 0xC8E6C9};
    if (index < 0 || index >= MULTIPLAYER_COUNT) return lv_color_hex(0x505050);
    return lv_color_hex(colors[index]);
}

static lv_color_t get_player_text_color(int index)
{
    return (index == 2) ? lv_color_black() : lv_color_white();
}

static lv_color_t get_player_preview_color(int index, int delta)
{
    if (index == 2) {
        return (delta < 0) ? lv_color_hex(0x7A1020) : lv_color_hex(0x215A2A);
    }
    return (delta < 0) ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
}

static void refresh_brightness_ring()
{
    lv_arc_set_value(arc_brightness, brightness_percent);

    lv_obj_set_style_arc_color(arc_brightness, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_brightness, 18, LV_PART_MAIN);

    lv_obj_set_style_arc_color(arc_brightness, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_brightness, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_brightness, true, LV_PART_INDICATOR);
}

static void refresh_settings_ui()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %d%%", brightness_percent);
    lv_label_set_text(label_settings_value, buf);
    refresh_brightness_ring();
    refresh_battery_ui();
}

static void refresh_multiplayer_ui()
{
    static const int16_t life_offsets_x[MULTIPLAYER_COUNT] = {-90, 90, 90, -90};
    static const int16_t life_offsets_y[MULTIPLAYER_COUNT] = {-90, -90, 90, 90};
    static const int16_t name_offsets_x[MULTIPLAYER_COUNT] = {-90, 90, 90, -90};
    static const int16_t name_offsets_y[MULTIPLAYER_COUNT] = {-40, -40, 32, 32};
    char buf[8];
    int i;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        bool preview_here = multiplayer_life_preview_active && (multiplayer_preview_player == i);
        lv_color_t text_color = get_player_text_color(i);

        if (multiplayer_quadrants[i] != NULL) {
            lv_obj_set_style_bg_color(
                multiplayer_quadrants[i],
                (i == multiplayer_selected) ? get_player_active_color(i) : get_player_base_color(i),
                0
            );
            lv_obj_set_style_bg_opa(multiplayer_quadrants[i], LV_OPA_COVER, 0);
        }

        if (label_multiplayer_life[i] != NULL) {
            if (preview_here) {
                snprintf(buf, sizeof(buf), "%+d", multiplayer_pending_life_delta);
                lv_label_set_text(label_multiplayer_life[i], buf);
                lv_obj_set_style_text_color(label_multiplayer_life[i], get_player_preview_color(i, multiplayer_pending_life_delta), 0);
            } else {
                snprintf(buf, sizeof(buf), "%d", multiplayer_life[i]);
                lv_label_set_text(label_multiplayer_life[i], buf);
                lv_obj_set_style_text_color(label_multiplayer_life[i], text_color, 0);
            }
            lv_obj_align(label_multiplayer_life[i], LV_ALIGN_CENTER, life_offsets_x[i], life_offsets_y[i]);
        }

        if (label_multiplayer_name[i] != NULL) {
            lv_label_set_text(label_multiplayer_name[i], multiplayer_names[i]);
            lv_obj_set_style_text_color(label_multiplayer_name[i], text_color, 0);
            lv_obj_align(label_multiplayer_name[i], LV_ALIGN_CENTER, name_offsets_x[i], name_offsets_y[i]);
        }
    }
}

static void refresh_multiplayer_menu_ui()
{
    char buf[32];
    bool player_menu;

    if (label_multiplayer_menu_title == NULL) return;

    player_menu = (multiplayer_menu_mode == MULTIPLAYER_MENU_PLAYER);

    if (player_menu) {
        snprintf(buf, sizeof(buf), "%s Menu", multiplayer_names[multiplayer_menu_player]);
    } else {
        snprintf(buf, sizeof(buf), "Multiplayer");
    }
    lv_label_set_text(label_multiplayer_menu_title, buf);

    if (button_multiplayer_menu_rename != NULL) {
        if (player_menu) lv_obj_clear_flag(button_multiplayer_menu_rename, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(button_multiplayer_menu_rename, LV_OBJ_FLAG_HIDDEN);
    }

    if (button_multiplayer_menu_cmd_damage != NULL) {
        if (player_menu) lv_obj_clear_flag(button_multiplayer_menu_cmd_damage, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(button_multiplayer_menu_cmd_damage, LV_OBJ_FLAG_HIDDEN);
    }

    if (button_multiplayer_menu_all_damage != NULL) {
        if (player_menu) lv_obj_add_flag(button_multiplayer_menu_all_damage, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(button_multiplayer_menu_all_damage, LV_OBJ_FLAG_HIDDEN);
    }

    if (button_multiplayer_menu_menu != NULL) {
        if (player_menu) lv_obj_add_flag(button_multiplayer_menu_menu, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(button_multiplayer_menu_menu, LV_OBJ_FLAG_HIDDEN);
    }

    if (button_multiplayer_menu_reset != NULL) {
        if (player_menu) lv_obj_add_flag(button_multiplayer_menu_reset, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(button_multiplayer_menu_reset, LV_OBJ_FLAG_HIDDEN);
    }

    if (button_multiplayer_menu_back != NULL) {
        lv_obj_clear_flag(button_multiplayer_menu_back, LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh_multiplayer_name_ui()
{
    char buf[40];

    if (label_multiplayer_name_title != NULL) {
        snprintf(buf, sizeof(buf), "Rename %s", multiplayer_names[multiplayer_menu_player]);
        lv_label_set_text(label_multiplayer_name_title, buf);
    }

    if (textarea_multiplayer_name != NULL) {
        lv_textarea_set_text(textarea_multiplayer_name, multiplayer_names[multiplayer_menu_player]);
    }
}

static void refresh_multiplayer_cmd_select_ui()
{
    char buf[48];
    int i;
    int row = 0;

    if (label_multiplayer_cmd_select_title != NULL) {
        snprintf(buf, sizeof(buf), "Source -> %s", multiplayer_names[multiplayer_menu_player]);
        lv_label_set_text(label_multiplayer_cmd_select_title, buf);
    }

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        if (i == multiplayer_menu_player) continue;
        multiplayer_cmd_target_choices[row] = i;
        if (button_multiplayer_cmd_target[row] != NULL) {
            lv_obj_clear_flag(button_multiplayer_cmd_target[row], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(button_multiplayer_cmd_target[row], get_player_base_color(i), 0);
            lv_obj_set_style_bg_opa(button_multiplayer_cmd_target[row], LV_OPA_COVER, 0);
        }
        if (label_multiplayer_cmd_target[row] != NULL) {
            snprintf(buf, sizeof(buf), "%s  %d", multiplayer_names[i], multiplayer_cmd_damage_totals[i][multiplayer_menu_player]);
            lv_label_set_text(label_multiplayer_cmd_target[row], buf);
            lv_obj_set_style_text_color(label_multiplayer_cmd_target[row], get_player_text_color(i), 0);
        }
        row++;
    }

    while (row < (MULTIPLAYER_COUNT - 1)) {
        multiplayer_cmd_target_choices[row] = -1;
        if (button_multiplayer_cmd_target[row] != NULL) {
            lv_obj_add_flag(button_multiplayer_cmd_target[row], LV_OBJ_FLAG_HIDDEN);
        }
        row++;
    }
}

static void refresh_multiplayer_cmd_damage_ui()
{
    char title_buf[48];
    char value_buf[32];

    if (multiplayer_cmd_target < 0 || multiplayer_cmd_target >= MULTIPLAYER_COUNT) return;

    if (label_multiplayer_cmd_damage_title != NULL) {
        snprintf(title_buf, sizeof(title_buf), "%s -> %s",
                 multiplayer_names[multiplayer_cmd_source],
                 multiplayer_names[multiplayer_cmd_target]);
        lv_label_set_text(label_multiplayer_cmd_damage_title, title_buf);
    }

    if (label_multiplayer_cmd_damage_value != NULL) {
        snprintf(value_buf, sizeof(value_buf), "Damage: %d", multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target]);
        lv_label_set_text(label_multiplayer_cmd_damage_value, value_buf);
    }
}

static void refresh_multiplayer_all_damage_ui()
{
    char buf[32];

    if (label_multiplayer_all_damage_title != NULL) {
        lv_label_set_text(label_multiplayer_all_damage_title, "All Players");
    }

    if (label_multiplayer_all_damage_value != NULL) {
        snprintf(buf, sizeof(buf), "Damage: %d", multiplayer_all_damage_value);
        lv_label_set_text(label_multiplayer_all_damage_value, buf);
    }
}

static void multiplayer_life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!multiplayer_life_preview_active ||
        multiplayer_preview_player < 0 ||
        multiplayer_preview_player >= MULTIPLAYER_COUNT) {
        if (multiplayer_life_preview_timer != NULL) {
            lv_timer_pause(multiplayer_life_preview_timer);
        }
        return;
    }

    multiplayer_life[multiplayer_preview_player] = clamp_life(
        multiplayer_life[multiplayer_preview_player] + multiplayer_pending_life_delta
    );
    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
    refresh_multiplayer_ui();
}

static void change_brightness(int delta)
{
    brightness_percent = clamp_brightness(brightness_percent + delta);
    knobby_platform_apply_brightness_percent(brightness_percent);
    refresh_settings_ui();
}

static void change_multiplayer_life(int delta)
{
    int preview_base;

    if (multiplayer_selected < 0 || multiplayer_selected >= MULTIPLAYER_COUNT) return;

    if (multiplayer_life_preview_active &&
        multiplayer_preview_player != multiplayer_selected) {
        multiplayer_life_preview_commit_cb(NULL);
    }

    multiplayer_preview_player = multiplayer_selected;
    preview_base = multiplayer_life[multiplayer_preview_player];
    multiplayer_pending_life_delta += delta;
    multiplayer_pending_life_delta = clamp_life(preview_base + multiplayer_pending_life_delta) - preview_base;
    multiplayer_life_preview_active = (multiplayer_pending_life_delta != 0);

    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_reset(multiplayer_life_preview_timer);
    }

    if (!multiplayer_life_preview_active && multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
        multiplayer_preview_player = -1;
    } else if (multiplayer_life_preview_timer != NULL) {
        lv_timer_resume(multiplayer_life_preview_timer);
    }

    refresh_multiplayer_ui();
}

static void change_multiplayer_cmd_damage(int delta)
{
    int updated_total;

    if (multiplayer_cmd_target < 0 || multiplayer_cmd_target >= MULTIPLAYER_COUNT) return;
    if (multiplayer_cmd_source < 0 || multiplayer_cmd_source >= MULTIPLAYER_COUNT) return;

    updated_total = multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] + delta;
    if (updated_total < 0) {
        delta = -multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target];
        updated_total = 0;
    }

    multiplayer_cmd_damage_totals[multiplayer_cmd_source][multiplayer_cmd_target] = updated_total;

    multiplayer_life[multiplayer_cmd_target] = clamp_life(multiplayer_life[multiplayer_cmd_target] - delta);
    refresh_multiplayer_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
}

static void change_multiplayer_all_damage(int delta)
{
    multiplayer_all_damage_value += delta;
    if (multiplayer_all_damage_value < 0) multiplayer_all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
}

static void reset_all_values(void)
{
    int i;

    multiplayer_pending_life_delta = 0;
    multiplayer_preview_player = -1;
    multiplayer_life_preview_active = false;
    brightness_percent = DEFAULT_BRIGHTNESS_PERCENT;

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = DEFAULT_LIFE_TOTAL;
        snprintf(multiplayer_names[i], sizeof(multiplayer_names[i]), "P%d", i + 1);
    }
    multiplayer_selected = 0;
    multiplayer_menu_player = 0;
    multiplayer_cmd_source = 0;
    multiplayer_cmd_target = -1;
    memset(multiplayer_cmd_damage_totals, 0, sizeof(multiplayer_cmd_damage_totals));
    multiplayer_all_damage_value = 0;

    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }

    knobby_platform_apply_brightness_percent(brightness_percent);
    refresh_settings_ui();
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_multiplayer_all_damage_ui();
}

static void intro_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (intro_step < INTRO_CHAR_COUNT) {
        intro_step++;
        refresh_intro_ui();
        return;
    }

    if (intro_timer != NULL) {
        lv_timer_pause(intro_timer);
    }
    open_multiplayer_screen();
}

// Screen navigation and transitions

static void flush_knob_input_queue(void)
{
    taskENTER_CRITICAL(&knob_event_queue_lock);
    knob_event_tail = knob_event_head;
    taskEXIT_CRITICAL(&knob_event_queue_lock);
}

static void load_screen_if_needed(lv_obj_t *screen)
{
    if (screen != NULL && lv_scr_act() != screen) {
        flush_knob_input_queue();
        lv_scr_load(screen);
    }
}

static void open_settings_screen()
{
    multiplayer_life_preview_commit_cb(NULL);
    update_battery_measurement(true);
    refresh_settings_ui();
    load_screen_if_needed(screen_settings);
}

static void open_multiplayer_screen()
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_ui();
    load_screen_if_needed(screen_multiplayer);
}

static void open_multiplayer_menu_screen(int player_index, multiplayer_menu_mode_t mode)
{
    multiplayer_life_preview_commit_cb(NULL);
    multiplayer_menu_player = player_index;
    multiplayer_menu_mode = mode;
    refresh_multiplayer_menu_ui();
    load_screen_if_needed(screen_multiplayer_menu);
}

static void open_multiplayer_name_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_name_ui();
    load_screen_if_needed(screen_multiplayer_name);
}

static void open_multiplayer_cmd_select_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_cmd_select_ui();
    load_screen_if_needed(screen_multiplayer_cmd_select);
}

static void open_multiplayer_cmd_damage_screen(int target_index)
{
    multiplayer_life_preview_commit_cb(NULL);
    multiplayer_cmd_source = target_index;
    multiplayer_cmd_target = multiplayer_menu_player;
    refresh_multiplayer_cmd_damage_ui();
    load_screen_if_needed(screen_multiplayer_cmd_damage);
}

static void open_multiplayer_all_damage_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    multiplayer_all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
    load_screen_if_needed(screen_multiplayer_all_damage);
}

static void event_menu_reset(lv_event_t *e)
{
    (void)e;
    reset_all_values();
    open_multiplayer_screen();
}

static void event_multiplayer_select(lv_event_t *e)
{
    int new_selected = (int)(intptr_t)lv_event_get_user_data(e);

    if (multiplayer_life_preview_active && multiplayer_preview_player != new_selected) {
        multiplayer_life_preview_commit_cb(NULL);
    }

    multiplayer_selected = new_selected;
    refresh_multiplayer_ui();
}

static void event_multiplayer_open_menu(lv_event_t *e)
{
    int new_selected = (int)(intptr_t)lv_event_get_user_data(e);

    if (multiplayer_life_preview_active && multiplayer_preview_player != new_selected) {
        multiplayer_life_preview_commit_cb(NULL);
    }

    multiplayer_selected = new_selected;
    refresh_multiplayer_ui();
    open_multiplayer_menu_screen(multiplayer_selected, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_swipe_menu(lv_event_t *e)
{
    lv_point_t point;
    lv_indev_t *indev = lv_indev_get_act();

    if (indev == NULL) return;

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        lv_indev_get_point(indev, &multiplayer_swipe_start);
        multiplayer_swipe_tracking = true;
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_RELEASED && multiplayer_swipe_tracking) {
        multiplayer_swipe_tracking = false;
        lv_indev_get_point(indev, &point);

        if ((multiplayer_swipe_start.y - point.y) > 80 &&
            LV_ABS(point.x - multiplayer_swipe_start.x) < 90) {
            open_multiplayer_menu_screen(multiplayer_selected, MULTIPLAYER_MENU_GLOBAL);
        }
    }
}

static void event_multiplayer_menu_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_screen();
}

static void event_multiplayer_menu_settings(lv_event_t *e)
{
    (void)e;
    open_settings_screen();
}

static void event_multiplayer_menu_rename(lv_event_t *e)
{
    (void)e;
    open_multiplayer_name_screen();
}

static void event_multiplayer_menu_cmd_damage(lv_event_t *e)
{
    (void)e;
    open_multiplayer_cmd_select_screen();
}

static void event_multiplayer_menu_all_damage(lv_event_t *e)
{
    (void)e;
    open_multiplayer_all_damage_screen();
}

static void event_multiplayer_name_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_name_save(lv_event_t *e)
{
    const char *txt;
    size_t len;

    (void)e;
    if (textarea_multiplayer_name == NULL) return;

    txt = lv_textarea_get_text(textarea_multiplayer_name);
    len = strlen(txt);
    if (len == 0) {
        snprintf(multiplayer_names[multiplayer_menu_player], sizeof(multiplayer_names[multiplayer_menu_player]),
                 "P%d", multiplayer_menu_player + 1);
    } else {
        snprintf(multiplayer_names[multiplayer_menu_player],
                 sizeof(multiplayer_names[multiplayer_menu_player]), "%s", txt);
    }

    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    open_multiplayer_menu_screen(multiplayer_menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_cmd_select_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_cmd_target_pick(lv_event_t *e)
{
    int row = (int)(intptr_t)lv_event_get_user_data(e);

    if (row < 0 || row >= (MULTIPLAYER_COUNT - 1)) return;
    if (multiplayer_cmd_target_choices[row] < 0) return;

    open_multiplayer_cmd_damage_screen(multiplayer_cmd_target_choices[row]);
}

static void event_multiplayer_cmd_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_cmd_select_screen();
}

static void event_multiplayer_all_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(multiplayer_menu_player, MULTIPLAYER_MENU_GLOBAL);
}

static void event_multiplayer_all_damage_apply(lv_event_t *e)
{
    int i;

    (void)e;
    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_life[i] = clamp_life(multiplayer_life[i] - multiplayer_all_damage_value);
    }

    refresh_multiplayer_ui();
    open_multiplayer_screen();
}

// Screen construction

static void build_intro_screen()
{
    int i;

    screen_intro = lv_obj_create(NULL);
    lv_obj_set_size(screen_intro, 360, 360);
    lv_obj_set_style_bg_color(screen_intro, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_intro, 0, 0);
    lv_obj_set_scrollbar_mode(screen_intro, LV_SCROLLBAR_MODE_OFF);

    for (i = 0; i < INTRO_CHAR_COUNT; i++) {
        intro_letters[i] = lv_label_create(screen_intro);
        lv_label_set_text(intro_letters[i], intro_text[i]);
        lv_obj_set_style_text_color(intro_letters[i], lv_color_hex(intro_colors[i]), 0);
        lv_obj_set_style_text_font(intro_letters[i], &lv_font_montserrat_32, 0);
        lv_obj_set_pos(intro_letters[i], intro_x[i], 146);
        lv_obj_add_flag(intro_letters[i], LV_OBJ_FLAG_HIDDEN);
    }

    refresh_intro_ui();
}

static void build_multiplayer_screen()
{
    static const char *player_names[MULTIPLAYER_COUNT] = {"P1", "P2", "P3", "P4"};
    static const lv_coord_t quad_x[MULTIPLAYER_COUNT] = {0, 180, 180, 0};
    static const lv_coord_t quad_y[MULTIPLAYER_COUNT] = {0, 0, 180, 180};
    int i;

    screen_multiplayer = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(screen_multiplayer, event_multiplayer_swipe_menu, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(screen_multiplayer, event_multiplayer_swipe_menu, LV_EVENT_RELEASED, NULL);

    for (i = 0; i < MULTIPLAYER_COUNT; i++) {
        multiplayer_quadrants[i] = lv_btn_create(screen_multiplayer);
        lv_obj_set_size(multiplayer_quadrants[i], 180, 180);
        lv_obj_set_pos(multiplayer_quadrants[i], quad_x[i], quad_y[i]);
        lv_obj_set_style_radius(multiplayer_quadrants[i], 0, 0);
        lv_obj_set_style_border_width(multiplayer_quadrants[i], 1, 0);
        lv_obj_set_style_border_color(multiplayer_quadrants[i], lv_color_black(), 0);
        lv_obj_set_style_shadow_width(multiplayer_quadrants[i], 0, 0);
        lv_obj_add_flag(multiplayer_quadrants[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_multiplayer_select, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_add_event_cb(multiplayer_quadrants[i], event_multiplayer_open_menu, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)i);

        label_multiplayer_name[i] = lv_label_create(screen_multiplayer);
        lv_label_set_text(label_multiplayer_name[i], player_names[i]);
        lv_obj_set_style_text_color(label_multiplayer_name[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(label_multiplayer_name[i], &lv_font_montserrat_14, 0);

        label_multiplayer_life[i] = lv_label_create(screen_multiplayer);
        lv_label_set_text(label_multiplayer_life[i], "40");
        lv_obj_set_style_text_color(label_multiplayer_life[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(label_multiplayer_life[i], &lv_font_montserrat_32, 0);
    }

    refresh_multiplayer_ui();
}

static void build_multiplayer_menu_screen()
{
    screen_multiplayer_menu = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_menu, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_menu, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer_menu, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_menu, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_menu_title = lv_label_create(screen_multiplayer_menu);
    lv_obj_set_style_text_color(label_multiplayer_menu_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_menu_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_menu_title, LV_ALIGN_TOP_MID, 0, 26);

    button_multiplayer_menu_rename = make_button(screen_multiplayer_menu, "Rename", 180, 44, event_multiplayer_menu_rename);
    lv_obj_align(button_multiplayer_menu_rename, LV_ALIGN_CENTER, 0, -56);

    button_multiplayer_menu_cmd_damage = make_button(screen_multiplayer_menu, "Commander", 180, 44, event_multiplayer_menu_cmd_damage);
    lv_obj_align(button_multiplayer_menu_cmd_damage, LV_ALIGN_CENTER, 0, -4);

    button_multiplayer_menu_all_damage = make_button(screen_multiplayer_menu, "Global", 180, 44, event_multiplayer_menu_all_damage);
    lv_obj_align(button_multiplayer_menu_all_damage, LV_ALIGN_CENTER, 0, -56);

    button_multiplayer_menu_menu = make_button(screen_multiplayer_menu, "Settings", 180, 44, event_multiplayer_menu_settings);
    lv_obj_align(button_multiplayer_menu_menu, LV_ALIGN_CENTER, 0, -4);

    button_multiplayer_menu_reset = make_button(screen_multiplayer_menu, "Reset", 180, 44, event_menu_reset);
    lv_obj_align(button_multiplayer_menu_reset, LV_ALIGN_CENTER, 0, 48);

    button_multiplayer_menu_back = make_button(screen_multiplayer_menu, "Back", 88, 42, event_multiplayer_menu_back);
    lv_obj_set_style_radius(button_multiplayer_menu_back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(button_multiplayer_menu_back, 0, 0);
    lv_obj_align(button_multiplayer_menu_back, LV_ALIGN_BOTTOM_MID, 0, -24);

    refresh_multiplayer_menu_ui();
}

static void build_multiplayer_name_screen()
{
    screen_multiplayer_name = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_name, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_name, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer_name, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_name, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_name_title = lv_label_create(screen_multiplayer_name);
    lv_obj_set_style_text_color(label_multiplayer_name_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_name_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_name_title, LV_ALIGN_TOP_MID, 0, 18);

    textarea_multiplayer_name = lv_textarea_create(screen_multiplayer_name);
    lv_obj_set_size(textarea_multiplayer_name, 240, 44);
    lv_obj_align(textarea_multiplayer_name, LV_ALIGN_TOP_MID, 0, 56);
    lv_textarea_set_max_length(textarea_multiplayer_name, 15);
    lv_textarea_set_one_line(textarea_multiplayer_name, true);

    keyboard_multiplayer_name = lv_keyboard_create(screen_multiplayer_name);
    lv_obj_set_size(keyboard_multiplayer_name, 360, 170);
    lv_obj_align(keyboard_multiplayer_name, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard_multiplayer_name, textarea_multiplayer_name);

    lv_obj_t *btn = make_button(screen_multiplayer_name, "Save", 88, 38, event_multiplayer_name_save);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -24, 116);

    btn = make_button(screen_multiplayer_name, "Back", 88, 38, event_multiplayer_name_back);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 24, 116);
}

static void build_multiplayer_cmd_select_screen()
{
    int i;

    screen_multiplayer_cmd_select = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_cmd_select, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_cmd_select, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer_cmd_select, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_cmd_select, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_cmd_select_title = lv_label_create(screen_multiplayer_cmd_select);
    lv_obj_set_style_text_color(label_multiplayer_cmd_select_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_select_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_cmd_select_title, LV_ALIGN_TOP_MID, 0, 22);

    for (i = 0; i < (MULTIPLAYER_COUNT - 1); i++) {
        button_multiplayer_cmd_target[i] = lv_btn_create(screen_multiplayer_cmd_select);
        lv_obj_set_size(button_multiplayer_cmd_target[i], 220, 46);
        lv_obj_align(button_multiplayer_cmd_target[i], LV_ALIGN_CENTER, 0, -42 + (i * 58));
        lv_obj_add_event_cb(button_multiplayer_cmd_target[i], event_multiplayer_cmd_target_pick, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        label_multiplayer_cmd_target[i] = lv_label_create(button_multiplayer_cmd_target[i]);
        lv_obj_set_style_text_font(label_multiplayer_cmd_target[i], &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(label_multiplayer_cmd_target[i], lv_color_white(), 0);
        lv_obj_center(label_multiplayer_cmd_target[i]);
    }

    lv_obj_t *btn_back = make_button(screen_multiplayer_cmd_select, "Back", 120, 46, event_multiplayer_cmd_select_back);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_multiplayer_cmd_damage_screen()
{
    screen_multiplayer_cmd_damage = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_cmd_damage, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_cmd_damage, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer_cmd_damage, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_cmd_damage, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_cmd_damage_title = lv_label_create(screen_multiplayer_cmd_damage);
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_cmd_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_cmd_damage_value = lv_label_create(screen_multiplayer_cmd_damage);
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_value, &lv_font_montserrat_32, 0);
    lv_obj_align(label_multiplayer_cmd_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_cmd_damage_hint = lv_label_create(screen_multiplayer_cmd_damage);
    lv_label_set_text(label_multiplayer_cmd_damage_hint, "Turn knob for damage");
    lv_obj_set_style_text_color(label_multiplayer_cmd_damage_hint, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_multiplayer_cmd_damage_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_cmd_damage_hint, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *btn_back = make_button(screen_multiplayer_cmd_damage, "Back", 120, 46, event_multiplayer_cmd_damage_back);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_multiplayer_all_damage_screen()
{
    screen_multiplayer_all_damage = lv_obj_create(NULL);
    lv_obj_set_size(screen_multiplayer_all_damage, 360, 360);
    lv_obj_set_style_bg_color(screen_multiplayer_all_damage, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_multiplayer_all_damage, 0, 0);
    lv_obj_set_scrollbar_mode(screen_multiplayer_all_damage, LV_SCROLLBAR_MODE_OFF);

    label_multiplayer_all_damage_title = lv_label_create(screen_multiplayer_all_damage);
    lv_obj_set_style_text_color(label_multiplayer_all_damage_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_multiplayer_all_damage_title, LV_ALIGN_TOP_MID, 0, 26);

    label_multiplayer_all_damage_value = lv_label_create(screen_multiplayer_all_damage);
    lv_obj_set_style_text_color(label_multiplayer_all_damage_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_value, &lv_font_montserrat_32, 0);
    lv_obj_align(label_multiplayer_all_damage_value, LV_ALIGN_CENTER, 0, -8);

    label_multiplayer_all_damage_hint = lv_label_create(screen_multiplayer_all_damage);
    lv_label_set_text(label_multiplayer_all_damage_hint, "Turn knob, then apply");
    lv_obj_set_style_text_color(label_multiplayer_all_damage_hint, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_multiplayer_all_damage_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_multiplayer_all_damage_hint, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t *btn = make_button(screen_multiplayer_all_damage, "Apply", 120, 46, event_multiplayer_all_damage_apply);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -78);

    btn = make_button(screen_multiplayer_all_damage, "Back", 120, 46, event_multiplayer_all_damage_back);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -24);
}

static void build_settings_screen()
{
    screen_settings = lv_obj_create(NULL);
    lv_obj_set_size(screen_settings, 360, 360);
    lv_obj_set_style_bg_color(screen_settings, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_settings, 0, 0);
    lv_obj_set_scrollbar_mode(screen_settings, LV_SCROLLBAR_MODE_OFF);

    arc_brightness = lv_arc_create(screen_settings);
    lv_obj_set_size(arc_brightness, 280, 280);
    lv_obj_align(arc_brightness, LV_ALIGN_CENTER, 0, -6);
    lv_arc_set_rotation(arc_brightness, 90);
    lv_arc_set_bg_angles(arc_brightness, 0, 360);
    lv_arc_set_range(arc_brightness, 0, 100);
    lv_arc_set_value(arc_brightness, brightness_percent);
    lv_obj_remove_style(arc_brightness, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_brightness, LV_OBJ_FLAG_CLICKABLE);

    label_settings_title = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_title, "knobby");
    lv_obj_set_style_text_color(label_settings_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_settings_title, &lv_font_montserrat_22, 0);
    lv_obj_align(label_settings_title, LV_ALIGN_TOP_MID, 0, 24);

    label_settings_value = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_value, "Brightness: 25%");
    lv_obj_set_style_text_color(label_settings_value, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_settings_value, &lv_font_montserrat_32, 0);
    lv_obj_align(label_settings_value, LV_ALIGN_CENTER, 0, -18);

    label_settings_battery = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_battery, "Battery: --%");
    lv_obj_set_style_text_color(label_settings_battery, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_text_font(label_settings_battery, &lv_font_montserrat_22, 0);
    lv_obj_align(label_settings_battery, LV_ALIGN_CENTER, 0, 16);

    label_settings_battery_detail = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_battery_detail, "No calibrated reading");
    lv_obj_set_style_text_color(label_settings_battery_detail, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_settings_battery_detail, &lv_font_montserrat_14, 0);
    lv_obj_align(label_settings_battery_detail, LV_ALIGN_CENTER, 0, 50);

    label_settings_hint = lv_label_create(screen_settings);
    lv_label_set_text(label_settings_hint, "Turn knob for brightness");
    lv_obj_set_style_text_color(label_settings_hint, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(label_settings_hint, &lv_font_montserrat_14, 0);
    lv_obj_align(label_settings_hint, LV_ALIGN_BOTTOM_MID, 0, -76);

    lv_obj_t *btn_back = make_button(screen_settings, "Back", 120, 52, event_multiplayer_menu_back);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void knob_gui(void)
{
    knobby_platform_apply_brightness_percent(brightness_percent);

    build_intro_screen();
    build_multiplayer_screen();
    build_multiplayer_menu_screen();
    build_multiplayer_name_screen();
    build_multiplayer_cmd_select_screen();
    build_multiplayer_cmd_damage_screen();
    build_multiplayer_all_damage_screen();
    build_settings_screen();

    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_multiplayer_all_damage_ui();
    refresh_settings_ui();

    intro_step = 0;
    refresh_intro_ui();
    intro_timer = lv_timer_create(intro_timer_cb, 500, NULL);
    multiplayer_life_preview_timer = lv_timer_create(multiplayer_life_preview_commit_cb, 4000, NULL);
    if (multiplayer_life_preview_timer != NULL) {
        lv_timer_pause(multiplayer_life_preview_timer);
    }
    lv_scr_load(screen_intro);
    if (intro_timer != NULL) {
        lv_timer_ready(intro_timer);
    }
}

// Encoder input routing
// Each detent applies one unit of life, commander damage, global damage, or brightness.
static void handle_knob_event(knob_event_t k)
{
    if (lv_scr_act() == screen_intro)
    {
        return;
    }
    else if (lv_scr_act() == screen_settings)
    {
        if (k == KNOB_LEFT)      change_brightness(-1);
        else if (k == KNOB_RIGHT) change_brightness(+1);
    }
    else if (lv_scr_act() == screen_multiplayer)
    {
        if (k == KNOB_LEFT)      change_multiplayer_life(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_life(+1);
    }
    else if (lv_scr_act() == screen_multiplayer_cmd_damage)
    {
        if (k == KNOB_LEFT)      change_multiplayer_cmd_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_cmd_damage(+1);
    }
    else if (lv_scr_act() == screen_multiplayer_all_damage)
    {
        if (k == KNOB_LEFT)      change_multiplayer_all_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_all_damage(+1);
    }
}

void knob_change(knob_event_t k, int cont)
{
    uint8_t next_head;
    uint8_t pending_count;

    if (!knob_initialized)
    {
        knob_initialized = true;
    }

    (void)cont;

    taskENTER_CRITICAL(&knob_event_queue_lock);
    next_head = (uint8_t)((knob_event_head + 1U) % KNOB_EVENT_QUEUE_SIZE);
    if (next_head == knob_event_tail) {
        knob_event_tail = (uint8_t)((knob_event_tail + 1U) % KNOB_EVENT_QUEUE_SIZE);
        knob_event_overflow_count++;
    }

    knob_event_queue[knob_event_head].event = k;
    knob_event_head = next_head;
    pending_count = knob_queue_count_unsafe();
    if (pending_count > knob_event_high_watermark) {
        knob_event_high_watermark = pending_count;
    }
    taskEXIT_CRITICAL(&knob_event_queue_lock);
}

void knob_process_pending(void)
{
    uint8_t processed = 0;
    uint8_t process_budget = KNOB_EVENT_PROCESS_BUDGET;

    taskENTER_CRITICAL(&knob_event_queue_lock);
    if (knob_queue_count_unsafe() > KNOB_EVENT_PROCESS_BUDGET) {
        process_budget = KNOB_EVENT_BURST_BUDGET;
    }
    taskEXIT_CRITICAL(&knob_event_queue_lock);

    while (processed < process_budget) {
        knob_event_t event;

        taskENTER_CRITICAL(&knob_event_queue_lock);
        if (knob_event_tail == knob_event_head) {
            taskEXIT_CRITICAL(&knob_event_queue_lock);
            break;
        }

        event = knob_event_queue[knob_event_tail].event;
        knob_event_tail = (uint8_t)((knob_event_tail + 1U) % KNOB_EVENT_QUEUE_SIZE);
        taskEXIT_CRITICAL(&knob_event_queue_lock);

        handle_knob_event(event);
        processed++;
    }

    report_knob_queue_status();
}
