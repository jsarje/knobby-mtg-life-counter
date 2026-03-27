// knob.cpp – LVGL event dispatcher and encoder input queue.
// Screen construction delegates to screen classes (screen_intro / screen_settings /
// screen_multiplayer).  Business rules delegate to domain controllers
// (multiplayer_controller).  Screen transitions delegate to the navigation
// coordinator (navigation_controller).

#include "knob.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "platform_services.h"
#include "session_state.h"
#include "multiplayer_controller.h"
#include "navigation_controller.h"
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

static const char *KNOB_GUI_TAG = "knob_gui";

// ---------------------------------------------------------------------------
// Module-level aliases for the session state singletons.
// All code in this file reads/writes gameplay and settings values through
// these references; there are no longer any competing file-scope statics for
// state that belongs to the session.
// ---------------------------------------------------------------------------
static MultiplayerGameState& mp = g_multiplayer_game_state;
static SettingsState&         ss = g_settings_state;

// Screen objects and all widget pointers are owned by the screen class
// instances declared in screen_intro.h / screen_settings.h /
// screen_multiplayer.h.

static lv_timer_t *intro_timer = NULL;
static lv_timer_t *multiplayer_life_preview_timer = NULL;
static uint8_t intro_step = 0;

static knob_input_event_t knob_event_queue[KNOB_EVENT_QUEUE_SIZE];
static volatile uint8_t knob_event_head = 0;
static volatile uint8_t knob_event_tail = 0;
static volatile uint32_t knob_event_overflow_count = 0;
static volatile uint8_t knob_event_high_watermark = 0;
static portMUX_TYPE knob_event_queue_lock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t knob_event_overflow_reported = 0;

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
static void refresh_multiplayer_player_count_ui() { g_screen_multiplayer_player_count.refresh(mp); }
static void refresh_multiplayer_player_count_confirm_ui() { g_screen_multiplayer_player_count_confirm.refresh(mp); }

static void refresh_all_multiplayer_ui()
{
    refresh_multiplayer_ui();
    refresh_multiplayer_menu_ui();
    refresh_multiplayer_name_ui();
    refresh_multiplayer_cmd_select_ui();
    refresh_multiplayer_cmd_damage_ui();
    refresh_multiplayer_all_damage_ui();
    refresh_multiplayer_player_count_ui();
    refresh_multiplayer_player_count_confirm_ui();
}

// Timer callback: commits any pending life-delta preview and refreshes the
// multiplayer screen so the committed total becomes visible.
static void multiplayer_life_preview_commit_cb(lv_timer_t *timer)
{
    (void)timer;
    g_multiplayer_controller.commitLifePreview();
    refresh_multiplayer_ui();
}

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
    g_multiplayer_controller.resetAll(ss);
    g_settings_controller.applyBrightness();
    refresh_settings_ui();
    refresh_all_multiplayer_ui();
}

static void apply_active_player_count_change(int new_count)
{
    if (!g_multiplayer_controller.canApplyActivePlayerCount(new_count)) {
        return;
    }

    if (new_count == mp.active_player_count) {
        mp.pending_player_count = -1;
        g_navigation_controller.openMultiplayerScreen();
        return;
    }

    if (g_multiplayer_controller.isSessionDirty()) {
        g_navigation_controller.openPlayerCountConfirmScreen(new_count);
        return;
    }

    mp.pending_player_count = -1;
    g_multiplayer_controller.setActivePlayerCount(new_count);
    refresh_all_multiplayer_ui();
    g_navigation_controller.openMultiplayerScreen();
}

static void confirm_active_player_count_change(void)
{
    const int new_count = mp.pending_player_count;
    if (!g_multiplayer_controller.canApplyActivePlayerCount(new_count)) {
        mp.pending_player_count = -1;
        g_navigation_controller.openPlayerCountScreen();
        return;
    }

    mp.pending_player_count = -1;
    g_multiplayer_controller.setActivePlayerCount(new_count);
    refresh_all_multiplayer_ui();
    g_navigation_controller.openMultiplayerScreen();
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
    g_navigation_controller.openMultiplayerScreen();
}

// Flushes stale encoder events when switching screens.  Passed to the
// NavigationController during knob_gui() initialisation.
static void knob_flush_input_queue(void)
{
    taskENTER_CRITICAL(&knob_event_queue_lock);
    knob_event_tail = knob_event_head;
    taskEXIT_CRITICAL(&knob_event_queue_lock);
}

// ---------------------------------------------------------------------------
// LVGL event callbacks – thin wrappers that forward to controller or
// navigation coordinator methods.
// ---------------------------------------------------------------------------

static void event_menu_reset(lv_event_t *e)
{
    (void)e;
    reset_all_values();
    g_navigation_controller.openMultiplayerScreen();
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
    g_navigation_controller.openMenuScreen(mp.selected, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_swipe_menu(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (indev == NULL) return;

    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
        g_navigation_controller.beginSwipe(point);
        return;
    }

    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        g_navigation_controller.endSwipe(point);
    }
}

static void event_multiplayer_menu_back(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openMultiplayerScreen();
}

static void event_multiplayer_menu_settings(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openSettingsScreen();
}

static void event_multiplayer_menu_players(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openPlayerCountScreen();
}

static void event_multiplayer_menu_rename(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openNameScreen();
}

static void event_multiplayer_menu_inc_commander_tax(lv_event_t *e)
{
    (void)e;
    // Increment commander tax for the menu player and refresh UI.
    g_multiplayer_controller.incrementCommanderTax(mp.menu_player);
    // Close the menu and return to the multiplayer screen so the user sees
    // the updated badge immediately.
    g_navigation_controller.openMultiplayerScreen();
}

static void event_multiplayer_menu_cmd_damage(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openCmdSelectScreen();
}

static void event_multiplayer_menu_all_damage(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openAllDamageScreen();
}

static void event_multiplayer_name_back(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openMenuScreen(mp.menu_player, MULTIPLAYER_MENU_PLAYER);
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
    g_navigation_controller.openMenuScreen(mp.menu_player, MULTIPLAYER_MENU_PLAYER);
}

static void event_multiplayer_cmd_select_back(lv_event_t *e)
{
    (void)e;
    // Return to main multiplayer screen instead of the long-press player menu.
    g_navigation_controller.openMultiplayerScreen();
}

static void event_multiplayer_cmd_target_pick(lv_event_t *e)
{
    int row = (int)(intptr_t)lv_event_get_user_data(e);
    const int choice = g_screen_multiplayer_cmd_select.targetChoice(row);
    if (choice < 0) return;
    g_navigation_controller.openCmdDamageScreen(choice);
}

static void event_multiplayer_cmd_damage_back(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openCmdSelectScreen();
}

static void event_multiplayer_all_damage_back(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openMenuScreen(mp.menu_player, MULTIPLAYER_MENU_GLOBAL);
}

static void event_multiplayer_all_damage_apply(lv_event_t *e)
{
    (void)e;
    g_multiplayer_controller.applyAllDamage();
    refresh_multiplayer_ui();
    g_navigation_controller.openMultiplayerScreen();
}

static void event_multiplayer_player_count_back(lv_event_t *e)
{
    (void)e;
    mp.pending_player_count = -1;
    g_navigation_controller.openMenuScreen(mp.selected, MULTIPLAYER_MENU_GLOBAL);
}

static void event_multiplayer_player_count_two(lv_event_t *e)
{
    (void)e;
    apply_active_player_count_change(2);
}

static void event_multiplayer_player_count_three(lv_event_t *e)
{
    (void)e;
    apply_active_player_count_change(3);
}

static void event_multiplayer_player_count_four(lv_event_t *e)
{
    (void)e;
    apply_active_player_count_change(4);
}

static void event_multiplayer_player_count_confirm_back(lv_event_t *e)
{
    (void)e;
    g_navigation_controller.openPlayerCountScreen();
}

static void event_multiplayer_player_count_confirm_apply(lv_event_t *e)
{
    (void)e;
    confirm_active_player_count_change();
}


// Screen classes (screen_intro, screen_settings, screen_multiplayer) own all
// widget pointers and LVGL object creation via their create() methods.
// NavigationController owns all lv_scr_load() decisions via openXxx() methods.


// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void knob_gui(void)
{
    g_settings_controller.applyBrightness();
    g_navigation_controller.init(knob_flush_input_queue);

    // Screen classes own construction (create()) and refresh().
    g_screen_intro.create();
    g_screen_multiplayer.create(
        event_multiplayer_select,
        event_multiplayer_open_menu,
        event_multiplayer_swipe_menu,
        event_multiplayer_swipe_menu);
    g_screen_multiplayer_menu.create(
        event_multiplayer_menu_rename,
        event_multiplayer_menu_cmd_damage,
        event_multiplayer_menu_inc_commander_tax,
        event_multiplayer_menu_all_damage,
        event_multiplayer_menu_players,
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
    g_screen_multiplayer_player_count.create(
        event_multiplayer_player_count_two,
        event_multiplayer_player_count_three,
        event_multiplayer_player_count_four,
        event_multiplayer_player_count_back);
    g_screen_multiplayer_player_count_confirm.create(
        event_multiplayer_player_count_confirm_apply,
        event_multiplayer_player_count_confirm_back);
    g_screen_settings.create(event_multiplayer_menu_back);

    refresh_all_multiplayer_ui();
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

void knob_change(knob_event_t k, int cont)
{
    uint8_t next_head;
    uint8_t pending_count;

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
