// navigation_controller.h
// Navigation coordinator that manages all screen transitions.
//
// NavigationController is the single boundary through which screen loads are
// requested.  It owns swipe-gesture tracking state and centralises the
// pre-transition work that was previously duplicated across every open_*()
// helper in knob.cpp: life-preview commit, per-screen refresh, input-queue
// flush, and lv_scr_load.
//
// Usage:
//   1. Call g_navigation_controller.init(flush_fn) once before any navigation.
//   2. Call openXxx() methods to transition between screens.
//   3. Forward LVGL touch PRESSED/RELEASED events on the multiplayer screen to
//      beginSwipe() / endSwipe() to enable the upward-swipe-to-menu gesture.

#pragma once

#include <lvgl.h>
#include "session_state.h"

class NavigationController {
public:
    // Callback type used to flush the encoder input queue before screen loads.
    using FlushFn = void(*)();

    // Registers the input-queue flush callback.  Must be called before any
    // openXxx() method is invoked.
    void init(FlushFn flush_fn);

    // Navigate to each screen.  Each method commits any pending life-delta
    // preview, performs the screen-specific state setup and refresh, flushes
    // stale encoder events, then calls lv_scr_load.
    void openMultiplayerScreen();
    void openSettingsScreen();
    void openMenuScreen(int player_index, MultiplayerMenuMode mode);
    void openNameScreen();
    void openCmdSelectScreen();
    void openCmdDamageScreen(int target_index);
    void openAllDamageScreen();
    void openPlayerCountScreen();
    void openPlayerCountConfirmScreen(int new_count);

    // Swipe-gesture tracking: forward LV_EVENT_PRESSED to beginSwipe and
    // LV_EVENT_RELEASED to endSwipe.  endSwipe opens the global menu when the
    // upward-swipe threshold is met.
    void beginSwipe(lv_point_t start);
    void endSwipe(lv_point_t end);

private:
    // Commits any active life-delta preview and refreshes the multiplayer
    // screen so committed totals are visible before transitioning away.
    void commitPreviewAndRefreshMultiplayer();

    // Flushes stale encoder events and calls lv_scr_load if the target screen
    // is not already active.
    void loadScreen(lv_obj_t* screen);

    FlushFn    flush_fn_       = nullptr;
    bool       swipe_tracking_ = false;
    lv_point_t swipe_start_    = {0, 0};
};

extern NavigationController g_navigation_controller;
