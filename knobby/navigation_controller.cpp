// navigation_controller.cpp
// Navigation coordinator implementation.

#include "navigation_controller.h"
#include "multiplayer_controller.h"
#include "screen_intro.h"
#include "screen_settings.h"
#include "screen_multiplayer.h"

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void NavigationController::init(FlushFn flush_fn) {
    flush_fn_ = flush_fn;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void NavigationController::commitPreviewAndRefreshMultiplayer() {
    g_multiplayer_controller.commitLifePreview();
    g_screen_multiplayer.refresh(g_multiplayer_game_state);
}

void NavigationController::loadScreen(lv_obj_t* screen) {
    if (screen == nullptr) return;
    if (lv_scr_act() == screen) return;
    if (flush_fn_) flush_fn_();
    lv_scr_load(screen);
}

// ---------------------------------------------------------------------------
// Public navigation methods
// ---------------------------------------------------------------------------

void NavigationController::openMultiplayerScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_screen_multiplayer.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer.lvObject());
}

void NavigationController::openSettingsScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_settings_controller.updateBattery(true);
    g_settings_state.battery_percent = g_settings_controller.readBatteryPercent();
    g_screen_settings.refresh(g_settings_state);
    loadScreen(g_screen_settings.lvObject());
}

void NavigationController::openMenuScreen(int player_index, MultiplayerMenuMode mode) {
    commitPreviewAndRefreshMultiplayer();
    g_multiplayer_game_state.menu_player = player_index;
    g_multiplayer_game_state.menu_mode   = mode;
    g_screen_multiplayer_menu.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_menu.lvObject());
}

void NavigationController::openNameScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_screen_multiplayer_name.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_name.lvObject());
}

void NavigationController::openCmdSelectScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_screen_multiplayer_cmd_select.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_cmd_select.lvObject());
}

void NavigationController::openCmdDamageScreen(int target_index) {
    commitPreviewAndRefreshMultiplayer();
    g_multiplayer_game_state.cmd_source = target_index;
    g_multiplayer_game_state.cmd_target = g_multiplayer_game_state.menu_player;
    g_screen_multiplayer_cmd_damage.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_cmd_damage.lvObject());
}

void NavigationController::openAllDamageScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_multiplayer_game_state.all_damage_value = 0;
    g_screen_multiplayer_all_damage.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_all_damage.lvObject());
}

void NavigationController::openPlayerCountScreen() {
    commitPreviewAndRefreshMultiplayer();
    g_screen_multiplayer_player_count.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_player_count.lvObject());
}

void NavigationController::openPlayerCountConfirmScreen(int new_count) {
    commitPreviewAndRefreshMultiplayer();
    if (new_count < kMinActivePlayerCount || new_count > kMultiplayerCount) return;
    g_multiplayer_game_state.pending_player_count = new_count;
    g_screen_multiplayer_player_count_confirm.refresh(g_multiplayer_game_state);
    loadScreen(g_screen_multiplayer_player_count_confirm.lvObject());
}

// ---------------------------------------------------------------------------
// Swipe-gesture tracking
// ---------------------------------------------------------------------------

void NavigationController::beginSwipe(lv_point_t start) {
    swipe_tracking_ = true;
    swipe_start_    = start;
}

void NavigationController::endSwipe(lv_point_t end) {
    if (!swipe_tracking_) return;
    swipe_tracking_ = false;
    if ((swipe_start_.y - end.y) > 80 &&
        LV_ABS(end.x - swipe_start_.x) < 90) {
        openMenuScreen(g_multiplayer_game_state.selected, MULTIPLAYER_MENU_GLOBAL);
    }
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

NavigationController g_navigation_controller;
