// screen_multiplayer.h
// Screen classes for all multiplayer-related LVGL screens.
//
// MultiplayerScreen        – the four-quadrant life-counter overview
// MultiplayerMenuScreen    – per-player and global action menu
// MultiplayerNameScreen    – player-name text input
// MultiplayerCmdSelectScreen – commander-damage source selection
// MultiplayerCmdDamageScreen – commander-damage adjustment
// MultiplayerAllDamageScreen – global damage application
//
// Each class owns its LVGL screen object and all child widgets; the file-
// scope global arrays in knob.cpp have been moved here.  LVGL event
// callbacks remain in knob.cpp and are passed in via create().

#pragma once

#include <lvgl.h>
#include "session_state.h"

// ---------------------------------------------------------------------------
// MultiplayerScreen
// ---------------------------------------------------------------------------

class MultiplayerScreen {
public:
    void create(lv_event_cb_t select_cb,
                lv_event_cb_t open_menu_cb,
                lv_event_cb_t swipe_pressed_cb,
                lv_event_cb_t swipe_released_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject() const { return screen_; }

    // Color helpers are public so sibling screen classes can share them.
    static lv_color_t baseColor(int index);
    static lv_color_t activeColor(int index);
    static lv_color_t textColor(int index);
    static lv_color_t previewColor(int index, int delta);

private:
    lv_obj_t* screen_                              = nullptr;
    lv_obj_t* quadrants_[kMultiplayerCount]        = {};
    lv_obj_t* label_life_[kMultiplayerCount]       = {};
    lv_obj_t* label_name_[kMultiplayerCount]       = {};
    lv_obj_t* label_commander_tax_[kMultiplayerCount] = {};
};

// ---------------------------------------------------------------------------
// MultiplayerMenuScreen
// ---------------------------------------------------------------------------

class MultiplayerMenuScreen {
public:
    void create(lv_event_cb_t rename_cb,
                lv_event_cb_t cmd_damage_cb,
                lv_event_cb_t inc_commander_cb,
                lv_event_cb_t all_damage_cb,
                lv_event_cb_t settings_cb,
                lv_event_cb_t reset_cb,
                lv_event_cb_t back_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject() const { return screen_; }

private:
    lv_obj_t* screen_                = nullptr;
    lv_obj_t* label_title_           = nullptr;
    lv_obj_t* btn_rename_            = nullptr;
    lv_obj_t* btn_cmd_damage_        = nullptr;
    lv_obj_t* btn_inc_commander_     = nullptr;
    lv_obj_t* btn_all_damage_        = nullptr;
    lv_obj_t* btn_menu_              = nullptr;
    lv_obj_t* btn_reset_             = nullptr;
    lv_obj_t* btn_back_              = nullptr;
};

// ---------------------------------------------------------------------------
// MultiplayerNameScreen
// ---------------------------------------------------------------------------

class MultiplayerNameScreen {
public:
    void create(lv_event_cb_t save_cb, lv_event_cb_t back_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject()    const { return screen_; }
    lv_obj_t* textarea()    const { return textarea_; }

private:
    lv_obj_t* screen_    = nullptr;
    lv_obj_t* label_title_ = nullptr;
    lv_obj_t* textarea_  = nullptr;
    lv_obj_t* keyboard_  = nullptr;
};

// ---------------------------------------------------------------------------
// MultiplayerCmdSelectScreen
// ---------------------------------------------------------------------------

class MultiplayerCmdSelectScreen {
public:
    void create(lv_event_cb_t target_pick_cb, lv_event_cb_t back_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject() const { return screen_; }

    // Returns the player index for a given button row, or -1 if the row is
    // unused.  Used by event_multiplayer_cmd_target_pick in knob.cpp.
    int targetChoice(int row) const {
        if (row < 0 || row >= kMultiplayerCount - 1) return -1;
        return target_choices_[row];
    }

private:
    lv_obj_t* screen_                              = nullptr;
    lv_obj_t* label_title_                         = nullptr;
    lv_obj_t* btn_target_[kMultiplayerCount - 1]   = {};
    lv_obj_t* label_target_[kMultiplayerCount - 1] = {};

    // Populated by refresh(); maps button rows to player indices.
    int target_choices_[kMultiplayerCount - 1]     = {-1, -1, -1};
};

// ---------------------------------------------------------------------------
// MultiplayerCmdDamageScreen
// ---------------------------------------------------------------------------

class MultiplayerCmdDamageScreen {
public:
    void create(lv_event_cb_t back_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject() const { return screen_; }

private:
    lv_obj_t* screen_      = nullptr;
    lv_obj_t* label_title_ = nullptr;
    lv_obj_t* label_value_ = nullptr;
    lv_obj_t* label_hint_  = nullptr;
};

// ---------------------------------------------------------------------------
// MultiplayerAllDamageScreen
// ---------------------------------------------------------------------------

class MultiplayerAllDamageScreen {
public:
    void create(lv_event_cb_t apply_cb, lv_event_cb_t back_cb);
    void refresh(const MultiplayerGameState& state);
    lv_obj_t* lvObject() const { return screen_; }

private:
    lv_obj_t* screen_      = nullptr;
    lv_obj_t* label_title_ = nullptr;
    lv_obj_t* label_value_ = nullptr;
    lv_obj_t* label_hint_  = nullptr;
};

// ---------------------------------------------------------------------------
// Global instances
// ---------------------------------------------------------------------------

extern MultiplayerScreen           g_screen_multiplayer;
extern MultiplayerMenuScreen       g_screen_multiplayer_menu;
extern MultiplayerNameScreen       g_screen_multiplayer_name;
extern MultiplayerCmdSelectScreen  g_screen_multiplayer_cmd_select;
extern MultiplayerCmdDamageScreen  g_screen_multiplayer_cmd_damage;
extern MultiplayerAllDamageScreen  g_screen_multiplayer_all_damage;
