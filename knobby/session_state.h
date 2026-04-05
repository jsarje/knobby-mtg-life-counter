#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Structural constants shared between state, controller, and screen layers.
// ---------------------------------------------------------------------------

static constexpr int kMultiplayerCount         = 4;
static constexpr int kMinActivePlayerCount     = 1;
static constexpr int kDefaultActivePlayerCount = 4;
static constexpr int kDefaultLifeTotal         = 40;
static constexpr int kDefaultBrightnessPercent = 25;
static constexpr int kPlayerNameMaxLen         = 16;
static constexpr int kLifeMin                  = -999;
static constexpr int kLifeMax                  = 999;

// ---------------------------------------------------------------------------
// Navigation / menu mode enum (used in MultiplayerGameState and knob.cpp).
// Kept as an unscoped enum so existing MULTIPLAYER_MENU_PLAYER / _GLOBAL
// identifiers continue to compile without change.
// ---------------------------------------------------------------------------

typedef enum {
    MULTIPLAYER_MENU_PLAYER = 0,
    MULTIPLAYER_MENU_GLOBAL = 1
} MultiplayerMenuMode;

typedef enum {
    MULTIPLAYER_ORIENTATION_SAME_DIRECTION = 0,
    MULTIPLAYER_ORIENTATION_OPPOSITE_SIDES = 1,
    MULTIPLAYER_ORIENTATION_ROUND_TABLE = 2
} MultiplayerOrientationMode;

static constexpr MultiplayerOrientationMode kDefaultOrientationMode =
    MULTIPLAYER_ORIENTATION_OPPOSITE_SIDES;

inline bool isValidOrientationMode(MultiplayerOrientationMode mode) {
    return mode >= MULTIPLAYER_ORIENTATION_SAME_DIRECTION &&
           mode <= MULTIPLAYER_ORIENTATION_ROUND_TABLE;
}

inline MultiplayerOrientationMode normalizeOrientationForPlayerCount(
    int player_count,
    MultiplayerOrientationMode mode)
{
    if (!isValidOrientationMode(mode)) {
        mode = kDefaultOrientationMode;
    }

    if (player_count <= 1) {
        return MULTIPLAYER_ORIENTATION_SAME_DIRECTION;
    }

    return mode;
}

// ---------------------------------------------------------------------------
// SettingsState – owns brightness and battery presentation values.
// ---------------------------------------------------------------------------

struct SettingsState {
    int      brightness_percent  = kDefaultBrightnessPercent;
    int      battery_percent     = -1;
    float    battery_voltage     = 0.0f;
    uint32_t battery_sample_tick = 0;
    bool     battery_sample_valid = false;

    // Resets user-facing settings to defaults.
    // Battery cache is intentionally left untouched because it reflects
    // hardware state, not game session state.
    void reset() {
        brightness_percent = kDefaultBrightnessPercent;
    }
};

// ---------------------------------------------------------------------------
// MultiplayerGameState – owns all runtime gameplay values for a session.
// ---------------------------------------------------------------------------

struct MultiplayerGameState {
    int              active_player_count = kDefaultActivePlayerCount;
    MultiplayerOrientationMode orientation_mode = kDefaultOrientationMode;
    int              life[kMultiplayerCount];
    int              commander_tax[kMultiplayerCount];
    int              selected          = 0;
    int              menu_player       = 0;
    int              pending_player_count = -1;
    MultiplayerOrientationMode pending_orientation_mode = kDefaultOrientationMode;
    bool             pending_orientation_valid = false;
    int              cmd_source        = 0;
    int              cmd_target        = -1;
    int              cmd_damage_totals[kMultiplayerCount][kMultiplayerCount];
    int              all_damage_value  = 0;
    MultiplayerMenuMode menu_mode      = MULTIPLAYER_MENU_PLAYER;
    int              pending_life_delta = 0;
    int              preview_player    = -1;
    bool             life_preview_active = false;
    char             names[kMultiplayerCount][kPlayerNameMaxLen];

    MultiplayerGameState() {
        reset(kDefaultActivePlayerCount, kDefaultOrientationMode, true);
    }

    // Resets all gameplay values back to new-game defaults.
    void reset(int new_active_player_count = -1,
               MultiplayerOrientationMode new_orientation_mode = kDefaultOrientationMode,
               bool apply_orientation = false) {
        if (new_active_player_count >= kMinActivePlayerCount &&
            new_active_player_count <= kMultiplayerCount) {
            active_player_count = new_active_player_count;
        } else if (active_player_count < kMinActivePlayerCount ||
                   active_player_count > kMultiplayerCount) {
            active_player_count = kDefaultActivePlayerCount;
        }

        orientation_mode = normalizeOrientationForPlayerCount(
            active_player_count,
            apply_orientation ? new_orientation_mode : orientation_mode);

        for (int i = 0; i < kMultiplayerCount; ++i) {
            life[i] = kDefaultLifeTotal;
            commander_tax[i] = 0;
            snprintf(names[i], kPlayerNameMaxLen, "P%d", i + 1);
        }
        selected          = 0;
        menu_player       = 0;
        pending_player_count = -1;
        pending_orientation_mode = orientation_mode;
        pending_orientation_valid = false;
        cmd_source        = 0;
        cmd_target        = -1;
        memset(cmd_damage_totals, 0, sizeof(cmd_damage_totals));
        all_damage_value  = 0;
        pending_life_delta = 0;
        preview_player    = -1;
        life_preview_active = false;
    }

    bool isActivePlayerIndex(int index) const {
        return index >= 0 && index < active_player_count;
    }

    int clampPlayerIndex(int index) const {
        if (active_player_count <= 0) return 0;
        if (index < 0) return 0;
        if (index >= active_player_count) return active_player_count - 1;
        return index;
    }
};

// ---------------------------------------------------------------------------
// Global state instances – defined in session_state.cpp.
// All other modules access gameplay and settings state through these objects.
// ---------------------------------------------------------------------------

extern SettingsState          g_settings_state;
extern MultiplayerGameState   g_multiplayer_game_state;
