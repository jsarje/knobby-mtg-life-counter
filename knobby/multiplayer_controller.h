// multiplayer_controller.h
// Domain controller layer.
//
// SettingsController owns brightness-adjustment and battery-measurement rules.
// MultiplayerController owns life-total, commander-damage, selection-change,
// preview-commit, and reset rules.
//
// Both controllers read and write exclusively through the SettingsState /
// MultiplayerGameState objects from session_state.h.  LVGL widget mutation
// remains in knob.cpp; controllers never reference widget pointers directly.

#pragma once

#include "session_state.h"
#include "platform_services.h"

#include <lvgl.h>

// ---------------------------------------------------------------------------
// SettingsController – brightness adjustment and battery measurement rules.
// ---------------------------------------------------------------------------

class SettingsController {
public:
    explicit SettingsController(SettingsState& state);

    // Adjusts brightness by delta steps, clamps to [5, 100], and applies the
    // new value to the backlight hardware.
    void adjustBrightness(int delta);

    // Reads and caches the battery voltage in state.  Pass force=true to
    // bypass the 5-second cooldown.
    void updateBattery(bool force);

    // Returns the battery percentage derived from the cached voltage, or -1
    // if no valid sample is available.
    int readBatteryPercent() const;

    // Applies the current brightness setting to the backlight hardware.
    void applyBrightness() const;

private:
    SettingsState& state_;

    // Battery discharge curve: voltage thresholds and matching percentages.
    static const float kCurveVoltages[];
    static const int   kCurvePercentages[];
    static constexpr int kCurvePoints = 9;

    static int  clampBrightness(int value);
    static int  clampPercent(int value);
    static int  percentFromVoltage(float voltage);
};

// ---------------------------------------------------------------------------
// MultiplayerController – life-total, damage, selection, and reset rules.
// ---------------------------------------------------------------------------

class MultiplayerController {
public:
    explicit MultiplayerController(MultiplayerGameState& state);

    // Registers the LVGL timer used to commit pending life-delta previews.
    // Must be called after the timer has been created in knob.cpp.
    void setPreviewTimer(lv_timer_t* timer);

    // Changes the currently-selected player's life total by delta.
    // Updates the preview state and manages the preview-commit timer.
    void adjustLife(int delta);

    // Commits any pending life-delta preview to the canonical life total.
    // Safe to call when no preview is active (no-op).
    void commitLifePreview();

    // Applies commander damage: debits delta from source→target damage total
    // and deducts the same amount from the target player's life total.
    // Delta is clamped so the damage total never goes negative.
    void adjustCmdDamage(int delta);

    // Accumulates global damage that will be applied to all players at once.
    void adjustAllDamage(int delta);

    // Applies the accumulated all-player damage to each player's life total,
    // then resets the accumulated counter.
    void applyAllDamage();

    // Changes the selected player.  Commits any preview for the previous
    // player if it was for a different player.
    void selectPlayer(int player_index);

    // Saves a new name for the current menu_player.  Resets to "P<n>" when
    // the provided name is empty.
    void saveName(const char* name);

    // Resets all multiplayer and settings state to new-game defaults.
    void resetAll(SettingsState& settings);

private:
    MultiplayerGameState& state_;
    lv_timer_t*           preview_timer_ = nullptr;

    static int clampLife(int value);
};

// ---------------------------------------------------------------------------
// Global controller instances – defined in multiplayer_controller.cpp.
// ---------------------------------------------------------------------------

extern SettingsController     g_settings_controller;
extern MultiplayerController  g_multiplayer_controller;
