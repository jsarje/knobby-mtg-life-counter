// screen_settings.h
// SettingsScreen class encapsulates the settings screen UI.
// Owns the LVGL screen object, brightness arc, and all setting labels.

#pragma once

#include <lvgl.h>
#include "session_state.h"

class SettingsScreen {
public:
    // Creates all LVGL objects.  back_cb is registered on the "Back" button.
    void create(lv_event_cb_t back_cb);

    // Refreshes all displayed values from state.
    void refresh(const SettingsState& state);

    // Returns the underlying LVGL screen object for lv_scr_load().
    lv_obj_t* lvObject() const { return screen_; }

private:
    void refreshBrightnessRing(int brightness_percent);
    void refreshBattery(const SettingsState& state);

    lv_obj_t* screen_               = nullptr;
    lv_obj_t* label_title_          = nullptr;
    lv_obj_t* label_value_          = nullptr;
    lv_obj_t* label_hint_           = nullptr;
    lv_obj_t* label_battery_        = nullptr;
    lv_obj_t* label_battery_detail_ = nullptr;
    lv_obj_t* arc_brightness_       = nullptr;
};

extern SettingsScreen g_screen_settings;
