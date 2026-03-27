// screen_settings.cpp
// Phase 5: SettingsScreen implementation.

#include "screen_settings.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// SettingsScreen methods
// ---------------------------------------------------------------------------

void SettingsScreen::create(lv_event_cb_t back_cb)
{
    char brightness_label[32];

    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    arc_brightness_ = lv_arc_create(screen_);
    lv_obj_set_size(arc_brightness_, 280, 280);
    lv_obj_align(arc_brightness_, LV_ALIGN_CENTER, 0, -6);
    lv_arc_set_rotation(arc_brightness_, 90);
    lv_arc_set_bg_angles(arc_brightness_, 0, 360);
    lv_arc_set_range(arc_brightness_, 0, 100);
    lv_arc_set_value(arc_brightness_, kDefaultBrightnessPercent);
    lv_obj_remove_style(arc_brightness_, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_brightness_, LV_OBJ_FLAG_CLICKABLE);

    label_title_ = lv_label_create(screen_);
    lv_label_set_text(label_title_, "knobby");
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 24);

    label_value_ = lv_label_create(screen_);
    snprintf(brightness_label, sizeof(brightness_label), "Brightness: %d%%", kDefaultBrightnessPercent);
    lv_label_set_text(label_value_, brightness_label);
    lv_obj_set_style_text_color(label_value_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_value_, &lv_font_montserrat_32, 0);
    lv_obj_align(label_value_, LV_ALIGN_CENTER, 0, -18);

    label_battery_ = lv_label_create(screen_);
    lv_label_set_text(label_battery_, "Battery: --%");
    lv_obj_set_style_text_color(label_battery_, lv_color_hex(0xB8B8B8), 0);
    lv_obj_set_style_text_font(label_battery_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_battery_, LV_ALIGN_CENTER, 0, 16);

    label_battery_detail_ = lv_label_create(screen_);
    lv_label_set_text(label_battery_detail_, "No calibrated reading");
    lv_obj_set_style_text_color(label_battery_detail_, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_battery_detail_, &lv_font_montserrat_14, 0);
    lv_obj_align(label_battery_detail_, LV_ALIGN_CENTER, 0, 50);

    label_hint_ = lv_label_create(screen_);
    lv_label_set_text(label_hint_, "Turn knob for brightness");
    lv_obj_set_style_text_color(label_hint_, lv_color_hex(0x6A6A6A), 0);
    lv_obj_set_style_text_font(label_hint_, &lv_font_montserrat_14, 0);
    lv_obj_align(label_hint_, LV_ALIGN_BOTTOM_MID, 0, -76);

    lv_obj_t* btn_back = lv_btn_create(screen_);
    lv_obj_set_size(btn_back, 120, 52);
    lv_obj_add_event_cb(btn_back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_t* lbl = lv_label_create(btn_back);
    lv_label_set_text(lbl, "Back");
    lv_obj_center(lbl);
}

void SettingsScreen::refresh(const SettingsState& state)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %d%%", state.brightness_percent);
    if (label_value_ != nullptr) lv_label_set_text(label_value_, buf);

    refreshBrightnessRing(state.brightness_percent);
    refreshBattery(state);
}

void SettingsScreen::refreshBrightnessRing(int brightness_percent)
{
    if (arc_brightness_ == nullptr) return;

    lv_arc_set_value(arc_brightness_, brightness_percent);

    lv_obj_set_style_arc_color(arc_brightness_, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_brightness_, 18, LV_PART_MAIN);

    lv_obj_set_style_arc_color(arc_brightness_, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_brightness_, 18, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_brightness_, true, LV_PART_INDICATOR);
}

void SettingsScreen::refreshBattery(const SettingsState& state)
{
    char buf[32];
    char detail_buf[48];

    if (label_battery_ == nullptr) return;

    if (state.battery_percent < 0) {
        lv_label_set_text(label_battery_, "Battery: --%");
        if (label_battery_detail_ != nullptr) {
            lv_label_set_text(label_battery_detail_, "No calibrated reading");
        }
        return;
    }

    snprintf(buf, sizeof(buf), "Battery: %d%%", state.battery_percent);
    lv_label_set_text(label_battery_, buf);
    if (label_battery_detail_ != nullptr) {
        snprintf(detail_buf, sizeof(detail_buf), "%.2fV calibrated", state.battery_voltage);
        lv_label_set_text(label_battery_detail_, detail_buf);
    }
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

SettingsScreen g_screen_settings;
