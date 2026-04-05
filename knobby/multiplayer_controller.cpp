// multiplayer_controller.cpp
// Business-rule logic for settings and multiplayer gameplay.
//
// SettingsController owns brightness and battery rules.
// MultiplayerController owns life/damage rules, preview-commit timer
// management, player-selection handling, name saving, and game reset.

#include "multiplayer_controller.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// SettingsController
// ---------------------------------------------------------------------------

const float SettingsController::kCurveVoltages[kCurvePoints] = {
    3.35f, 3.55f, 3.68f, 3.74f, 3.80f, 3.88f, 3.96f, 4.06f, 4.18f
};

const int SettingsController::kCurvePercentages[kCurvePoints] = {
    0, 5, 12, 22, 34, 48, 64, 82, 100
};

SettingsController::SettingsController(SettingsState& state)
    : state_(state) {}

void SettingsController::adjustBrightness(int delta) {
    state_.brightness_percent = clampBrightness(state_.brightness_percent + delta);
    applyBrightness();
}

void SettingsController::applyBrightness() const {
    knobby_platform_apply_brightness_percent(state_.brightness_percent);
}

void SettingsController::updateBattery(bool force) {
    if (!force && state_.battery_sample_valid &&
        (lv_tick_elaps(state_.battery_sample_tick) < 5000)) {
        return;
    }
    state_.battery_voltage     = knob_read_battery_voltage();
    state_.battery_sample_tick = lv_tick_get();
    state_.battery_sample_valid = (state_.battery_voltage > 0.0f);
}

int SettingsController::readBatteryPercent() const {
    if (!state_.battery_sample_valid) return -1;
    return percentFromVoltage(state_.battery_voltage);
}

int SettingsController::clampBrightness(int value) {
    if (value < 5)   return 5;
    if (value > 100) return 100;
    return value;
}

int SettingsController::clampPercent(int value) {
    if (value < 0)   return 0;
    if (value > 100) return 100;
    return value;
}

int SettingsController::percentFromVoltage(float voltage) {
    if (voltage <= kCurveVoltages[0]) return 0;
    for (int i = 1; i < kCurvePoints; ++i) {
        if (voltage <= kCurveVoltages[i]) {
            float low_v  = kCurveVoltages[i - 1];
            float high_v = kCurveVoltages[i];
            int   low_p  = kCurvePercentages[i - 1];
            int   high_p = kCurvePercentages[i];
            float ratio  = (voltage - low_v) / (high_v - low_v);
            return clampPercent(static_cast<int>(low_p + ((high_p - low_p) * ratio) + 0.5f));
        }
    }
    return 100;
}

// ---------------------------------------------------------------------------
// MultiplayerController
// ---------------------------------------------------------------------------

MultiplayerController::MultiplayerController(MultiplayerGameState& state)
    : state_(state) {}

void MultiplayerController::setPreviewTimer(lv_timer_t* timer) {
    preview_timer_ = timer;
}

void MultiplayerController::commitLifePreview() {
    if (!state_.life_preview_active ||
        state_.preview_player < 0 ||
        !state_.isActivePlayerIndex(state_.preview_player)) {
        if (preview_timer_ != nullptr) {
            lv_timer_pause(preview_timer_);
        }
        return;
    }

    state_.life[state_.preview_player] = clampLife(
        state_.life[state_.preview_player] + state_.pending_life_delta
    );
    state_.pending_life_delta  = 0;
    state_.preview_player      = -1;
    state_.life_preview_active = false;
    if (preview_timer_ != nullptr) {
        lv_timer_pause(preview_timer_);
    }
}

void MultiplayerController::adjustLife(int delta) {
    if (!state_.isActivePlayerIndex(state_.selected)) return;

    if (state_.life_preview_active && state_.preview_player != state_.selected) {
        commitLifePreview();
    }

    state_.preview_player = state_.selected;
    const int preview_base = state_.life[state_.preview_player];
    state_.pending_life_delta += delta;
    state_.pending_life_delta =
        clampLife(preview_base + state_.pending_life_delta) - preview_base;
    state_.life_preview_active = (state_.pending_life_delta != 0);

    if (preview_timer_ != nullptr) {
        lv_timer_reset(preview_timer_);
    }

    if (!state_.life_preview_active && preview_timer_ != nullptr) {
        lv_timer_pause(preview_timer_);
        state_.preview_player = -1;
    } else if (preview_timer_ != nullptr) {
        lv_timer_resume(preview_timer_);
    }
}

void MultiplayerController::adjustCmdDamage(int delta) {
    if (!isValidCmdDamagePair(state_.cmd_source, state_.cmd_target)) return;

    int updated = state_.cmd_damage_totals[state_.cmd_source][state_.cmd_target] + delta;
    if (updated < 0) {
        delta   = -state_.cmd_damage_totals[state_.cmd_source][state_.cmd_target];
        updated = 0;
    }
    state_.cmd_damage_totals[state_.cmd_source][state_.cmd_target] = updated;
    state_.life[state_.cmd_target] = clampLife(state_.life[state_.cmd_target] - delta);
}

void MultiplayerController::adjustAllDamage(int delta) {
    state_.all_damage_value += delta;
    if (state_.all_damage_value < 0) state_.all_damage_value = 0;
}

void MultiplayerController::applyAllDamage() {
    for (int i = 0; i < state_.active_player_count; ++i) {
        state_.life[i] = clampLife(state_.life[i] - state_.all_damage_value);
    }
    state_.all_damage_value = 0;
}

void MultiplayerController::selectPlayer(int player_index) {
    if (!state_.isActivePlayerIndex(player_index)) return;
    if (state_.life_preview_active && state_.preview_player != player_index) {
        commitLifePreview();
    }
    state_.selected = player_index;
}

void MultiplayerController::saveName(const char* name) {
    if (!isValidMenuPlayer()) return;

    const size_t len = strlen(name);
    if (len == 0) {
        snprintf(state_.names[state_.menu_player],
                 kPlayerNameMaxLen, "P%d", state_.menu_player + 1);
    } else {
        snprintf(state_.names[state_.menu_player], kPlayerNameMaxLen, "%s", name);
    }
}

bool MultiplayerController::isSessionDirty() const {
    if (state_.life_preview_active || state_.pending_life_delta != 0 || state_.all_damage_value != 0) {
        return true;
    }

    for (int i = 0; i < state_.active_player_count; ++i) {
        char default_name[kPlayerNameMaxLen];
        snprintf(default_name, sizeof(default_name), "P%d", i + 1);

        if (state_.life[i] != kDefaultLifeTotal) return true;
        if (state_.commander_tax[i] != 0) return true;
        if (strncmp(state_.names[i], default_name, kPlayerNameMaxLen) != 0) return true;
    }

    for (int source = 0; source < state_.active_player_count; ++source) {
        for (int target = 0; target < state_.active_player_count; ++target) {
            if (state_.cmd_damage_totals[source][target] != 0) return true;
        }
    }

    return false;
}

bool MultiplayerController::canApplyActivePlayerCount(int new_count) const {
    return isSupportedActivePlayerCount(new_count);
}

bool MultiplayerController::canApplyNewGameConfig(
    int player_count,
    MultiplayerOrientationMode orientation) const
{
    if (!isSupportedActivePlayerCount(player_count)) return false;
    return isValidOrientationMode(normalizeOrientation(player_count, orientation));
}

bool MultiplayerController::isCurrentNewGameConfig(
    int player_count,
    MultiplayerOrientationMode orientation) const
{
    if (!isSupportedActivePlayerCount(player_count)) return false;

    const MultiplayerOrientationMode normalized =
        normalizeOrientation(player_count, orientation);
    const MultiplayerOrientationMode current =
        normalizeOrientation(state_.active_player_count, state_.orientation_mode);

    return state_.active_player_count == player_count && current == normalized;
}

void MultiplayerController::stageNewGamePlayerCount(int player_count) {
    if (!isSupportedActivePlayerCount(player_count)) {
        state_.pending_player_count = -1;
        return;
    }

    if (player_count > 1 &&
        state_.pending_player_count == player_count &&
        state_.pending_orientation_valid) {
        state_.pending_player_count = player_count;
        state_.pending_orientation_mode = normalizeOrientation(
            player_count,
            state_.pending_orientation_mode);
        return;
    }

    state_.pending_player_count = player_count;
    if (player_count <= 1) {
        state_.pending_orientation_mode = normalizeOrientation(player_count, state_.orientation_mode);
        state_.pending_orientation_valid = true;
    } else {
        state_.pending_orientation_mode = normalizeOrientation(player_count, state_.orientation_mode);
        state_.pending_orientation_valid = false;
    }
}

void MultiplayerController::stageNewGameOrientation(MultiplayerOrientationMode orientation) {
    if (!isSupportedActivePlayerCount(state_.pending_player_count)) {
        state_.pending_orientation_valid = false;
        return;
    }

    state_.pending_orientation_mode = normalizeOrientation(state_.pending_player_count, orientation);
    state_.pending_orientation_valid = true;
}

void MultiplayerController::clearPendingNewGameConfig() {
    state_.pending_player_count = -1;
    state_.pending_orientation_mode = normalizeOrientation(
        state_.active_player_count,
        state_.orientation_mode);
    state_.pending_orientation_valid = false;
}

void MultiplayerController::applyNewGameConfig(
    int player_count,
    MultiplayerOrientationMode orientation)
{
    if (!canApplyNewGameConfig(player_count, orientation)) return;
    if (isCurrentNewGameConfig(player_count, orientation)) {
        clearPendingNewGameConfig();
        return;
    }

    state_.reset(player_count, normalizeOrientation(player_count, orientation), true);
    if (preview_timer_ != nullptr) {
        lv_timer_pause(preview_timer_);
    }
}

void MultiplayerController::setActivePlayerCount(int new_count) {
    if (!isSupportedActivePlayerCount(new_count)) return;
    if (new_count == state_.active_player_count) return;

    state_.reset(new_count, state_.orientation_mode, true);
    if (preview_timer_ != nullptr) {
        lv_timer_pause(preview_timer_);
    }
}

void MultiplayerController::resetAll(SettingsState& settings) {
    state_.reset(state_.active_player_count, state_.orientation_mode, true);
    settings.reset();
    if (preview_timer_ != nullptr) {
        lv_timer_pause(preview_timer_);
    }
}

void MultiplayerController::incrementCommanderTax(int player_index) {
    if (!state_.isActivePlayerIndex(player_index)) return;
    state_.commander_tax[player_index] += 1;
}

bool MultiplayerController::isValidCmdDamagePair(int source, int target) const {
    // Target must be an active player. In single-player sessions, allow
    // selecting commander-damage sources from inactive backing slots (P2..P4)
    // so a lone player can record damage received from those opponents.
    if (!state_.isActivePlayerIndex(target)) return false;

    if (state_.active_player_count == 1) {
        return (source >= 0 && source < kMultiplayerCount && source != target);
    }

    return state_.isActivePlayerIndex(source) && state_.isActivePlayerIndex(target);
}

bool MultiplayerController::isValidMenuPlayer() const {
    return state_.isActivePlayerIndex(state_.menu_player);
}

int MultiplayerController::clampLife(int value) {
    if (value < kLifeMin) return kLifeMin;
    if (value > kLifeMax) return kLifeMax;
    return value;
}

bool MultiplayerController::isSupportedActivePlayerCount(int value) {
    return value >= kMinActivePlayerCount && value <= kMultiplayerCount;
}

MultiplayerOrientationMode MultiplayerController::normalizeOrientation(
    int player_count,
    MultiplayerOrientationMode orientation)
{
    return normalizeOrientationForPlayerCount(player_count, orientation);
}

// ---------------------------------------------------------------------------
// Global controller instances.
// ---------------------------------------------------------------------------

SettingsController    g_settings_controller(g_settings_state);
MultiplayerController g_multiplayer_controller(g_multiplayer_game_state);
