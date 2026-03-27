// knob.cpp – Phase 4: business rules moved to controller classes.
// (was Phase 3: converted from knob.c with session-state extraction)
// Phase 5: screen construction/refresh moved to screen classes.
// LVGL event callbacks are now thin: they forward actions to controllers
// and then call screen-refresh helpers.

#include "knob.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "platform_services.h"
#include "session_state.h"
#include "multiplayer_controller.h"
#include "screen_intro.h"
#include "screen_settings.h"
#include "screen_multiplayer.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Structural constants – canonical definitions live in session_state.h.
#define INTRO_CHAR_COUNT      7
#define KNOB_EVENT_QUEUE_SIZE 32
#define KNOB_EVENT_PROCESS_BUDGET  8U
#define KNOB_EVENT_BURST_BUDGET   16U
#define KNOB_EVENT_DROP_LOG_INTERVAL 8U

typedef struct
{
    knob_event_t event;
} knob_input_event_t;

// MultiplayerMenuMode enum is now in session_state.h.

static const char *KNOB_GUI_TAG = "knob_gui";

// ---------------------------------------------------------------------------
// Module-level aliases for the session state singletons.
// All code in this file reads/writes gameplay and settings values through
// these references; there are no longer any competing file-scope statics for
// state that belongs to the session.
// ---------------------------------------------------------------------------
static MultiplayerGameState& mp = g_multiplayer_game_state;
static SettingsState&         ss = g_settings_state;

// Phase 5: Screen objects and all widget pointers are now owned by the
// screen class instances declared in screen_intro.h / screen_settings.h /
// screen_multiplayer.h.  The file-scope globals that previously held them
// have been removed.

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

// battery and multiplayer runtime state is now owned by g_settings_state
// and g_multiplayer_game_state (see session_state.h / session_state.cpp).
static uint8_t intro_step = 0;
// Swipe tracking is input state, not gameplay state; kept local here.
static bool multiplayer_swipe_tracking = false;
static lv_point_t multiplayer_swipe_start = {0, 0};

// Static UI assets moved to screen class files in Phase 5:
//  - intro_text / intro_colors / intro_x  →  screen_intro.cpp (IntroScreen)
//  - battery_curve                        →  multiplayer_controller.cpp (SettingsController)

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

// ---------------------------------------------------------------------------
// Phase 4: all domain helpers (clamp_*, battery_percent_from_voltage,
// update_battery_measurement, read_battery_percent) have been moved to
// SettingsController / MultiplayerController in multiplayer_controller.cpp.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Phase 5: screen refresh wrappers – thin calls into screen class refresh().
// make_button and player color helpers moved to screen_multiplayer.cpp.
// ---------------------------------------------------------------------------

static void refresh_intro_ui(void)
{
    g_screen_intro.refresh(intro_step);
}

static void refresh_settings_ui()
{
    // Ensure battery state is current before the screen reads from ss.
    ss.battery_percent = g_settings_controller.readBatteryPercent();
    g_screen_settings.refresh(ss);
}

static void refresh_multiplayer_ui()         { g_screen_multiplayer.refresh(mp); }
static void refresh_multiplayer_menu_ui()    { g_screen_multiplayer_menu.refresh(mp); }
static void refresh_multiplayer_name_ui()    { g_screen_multiplayer_name.refresh(mp); }
static void refresh_multiplayer_cmd_select_ui() { g_screen_multiplayer_cmd_select.refresh(mp); }
static void refresh_multiplayer_cmd_damage_ui() { g_screen_multiplayer_cmd_damage.refresh(mp); }
static void refresh_multiplayer_all_damage_ui() { g_screen_multiplayer_all_damage.refresh(mp); }

// Phase 4: timer callback forwards to MultiplayerController::commitLifePreview().
static void multiplayer_life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;
    g_multiplayer_controller.commitLifePreview();
    refresh_multiplayer_ui();
}

// Phase 4: thin wrappers – business rules now live in controller classes.
static void change_brightness(int delta)
{
    g_settings_controller.adjustBrightness(delta);
    refresh_settings_ui();
}

static void change_multiplayer_life(int delta)
{
    g_multiplayer_controller.adjustLife(delta);
    refresh_multiplayer_ui();
}

static void change_multiplayer_cmd_damage(int delta)
{
    g_multiplayer_controller.adjustCmdDamage(delta);
    refresh_multiplayer_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
}

static void change_multiplayer_all_damage(int delta)
{
    g_multiplayer_controller.adjustAllDamage(delta);
    refresh_multiplayer_all_damage_ui();
}

static void reset_all_values(void)
{
    // Phase 4: delegate to the controller which handles preview-timer cleanup.
    g_multiplayer_controller.resetAll(ss);
    g_settings_controller.applyBrightness();
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
    g_settings_controller.updateBattery(true);
    refresh_settings_ui();
    load_screen_if_needed(g_screen_settings.lvObject());
}

static void open_multiplayer_screen()
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_ui();
    load_screen_if_needed(g_screen_multiplayer.lvObject());
}

static void open_multiplayer_menu_screen(int player_index, MultiplayerMenuMode mode)
{
    multiplayer_life_preview_commit_cb(NULL);
    mp.menu_player = player_index;
    mp.menu_mode = mode;
    refresh_multiplayer_menu_ui();
    load_screen_if_needed(g_screen_multiplayer_menu.lvObject());
}

static void open_multiplayer_name_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_name_ui();
    load_screen_if_needed(g_screen_multiplayer_name.lvObject());
}

static void open_multiplayer_cmd_select_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    refresh_multiplayer_cmd_select_ui();
    load_screen_if_needed(g_screen_multiplayer_cmd_select.lvObject());
}

static void open_multiplayer_cmd_damage_screen(int target_index)
{
    multiplayer_life_preview_commit_cb(NULL);
    mp.cmd_source = target_index;
    mp.cmd_target = mp.menu_player;
    refresh_multiplayer_cmd_damage_ui();
    load_screen_if_needed(g_screen_multiplayer_cmd_damage.lvObject());
}

static void open_multiplayer_all_damage_screen(void)
{
    multiplayer_life_preview_commit_cb(NULL);
    mp.all_damage_value = 0;
    refresh_multiplayer_all_damage_ui();
    load_screen_if_needed(g_screen_multiplayer_all_damage.lvObject());
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
    g_multiplayer_controller.selectPlayer(new_selected);
    refresh_multiplayer_ui();
}

static void event_multiplayer_open_menu(lv_event_t *e)
{
    int new_selected = (int)(intptr_t)lv_event_get_user_data(e);
    g_multiplayer_controller.selectPlayer(new_selected);
    refresh_multiplayer_ui();
    open_multiplayer_menu_screen(mp.selected, MULTIPLAYER_MENU_PLAYER);
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
            open_multiplayer_menu_screen(mp.selected, MULTIPLAYER_MENU_GLOBAL);
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
    open_multiplayer_menu_screen(mp.menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_name_save(lv_event_t *e)
{
    (void)e;
    if (g_screen_multiplayer_name.textarea() == nullptr) return;

    g_multiplayer_controller.saveName(lv_textarea_get_text(g_screen_multiplayer_name.textarea()));

    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    open_multiplayer_menu_screen(mp.menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_cmd_select_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(mp.menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_cmd_target_pick(lv_event_t *e)
{
    int row = (int)(intptr_t)lv_event_get_user_data(e);
    const int choice = g_screen_multiplayer_cmd_select.targetChoice(row);
    if (choice < 0) return;
    open_multiplayer_cmd_damage_screen(choice);
}

static void event_multiplayer_cmd_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_cmd_select_screen();
}

static void event_multiplayer_all_damage_back(lv_event_t *e)
{
    (void)e;
    open_multiplayer_menu_screen(mp.menu_player, MULTIPLAYER_MENU_GLOBAL);
}

static void event_multiplayer_all_damage_apply(lv_event_t *e)
{
    (void)e;
    g_multiplayer_controller.applyAllDamage();
    refresh_multiplayer_ui();
    open_multiplayer_screen();
}


// Screen construction – Phase 5: build_* functions removed.
// Screen classes (screen_intro, screen_settings, screen_multiplayer)
// own all widget pointers and LVGL object creation.  See knob_gui()
// below for the create() call sequence.


// ---------------------------------------------------------------------------
// Public C-linkage API – definitions match the extern "C" declarations in
// knob.h so that callers compiled as C (or C++) use the C ABI.
// ---------------------------------------------------------------------------

extern "C" void knob_gui(void)
{
    g_settings_controller.applyBrightness();

    // Phase 5: screen classes own construction (create()) and refresh().
    g_screen_intro.create();
    g_screen_multiplayer.create(
        event_multiplayer_select,
        event_multiplayer_open_menu,
        event_multiplayer_swipe_menu,
        event_multiplayer_swipe_menu);
    g_screen_multiplayer_menu.create(
        event_multiplayer_menu_rename,
        event_multiplayer_menu_cmd_damage,
        event_multiplayer_menu_all_damage,
        event_multiplayer_menu_settings,
        event_menu_reset,
        event_multiplayer_menu_back);
    g_screen_multiplayer_name.create(
        event_multiplayer_name_save,
        event_multiplayer_name_back);
    g_screen_multiplayer_cmd_select.create(
        event_multiplayer_cmd_target_pick,
        event_multiplayer_cmd_select_back);
    g_screen_multiplayer_cmd_damage.create(event_multiplayer_cmd_damage_back);
    g_screen_multiplayer_all_damage.create(
        event_multiplayer_all_damage_apply,
        event_multiplayer_all_damage_back);
    g_screen_settings.create(event_multiplayer_menu_back);

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
        g_multiplayer_controller.setPreviewTimer(multiplayer_life_preview_timer);
    }
    lv_scr_load(g_screen_intro.lvObject());
    if (intro_timer != NULL) {
        lv_timer_ready(intro_timer);
    }
}

// Encoder input routing
// Each detent applies one unit of life, commander damage, global damage, or brightness.
static void handle_knob_event(knob_event_t k)
{
    if (lv_scr_act() == g_screen_intro.lvObject())
    {
        return;
    }
    else if (lv_scr_act() == g_screen_settings.lvObject())
    {
        if (k == KNOB_LEFT)      change_brightness(-1);
        else if (k == KNOB_RIGHT) change_brightness(+1);
    }
    else if (lv_scr_act() == g_screen_multiplayer.lvObject())
    {
        if (k == KNOB_LEFT)      change_multiplayer_life(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_life(+1);
    }
    else if (lv_scr_act() == g_screen_multiplayer_cmd_damage.lvObject())
    {
        if (k == KNOB_LEFT)      change_multiplayer_cmd_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_cmd_damage(+1);
    }
    else if (lv_scr_act() == g_screen_multiplayer_all_damage.lvObject())
    {
        if (k == KNOB_LEFT)      change_multiplayer_all_damage(-1);
        else if (k == KNOB_RIGHT) change_multiplayer_all_damage(+1);
    }
}

extern "C" void knob_change(knob_event_t k, int cont)
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

extern "C" void knob_process_pending(void)
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
