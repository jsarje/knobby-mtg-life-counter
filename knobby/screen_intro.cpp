// screen_intro.cpp
// Phase 5: IntroScreen implementation.

#include "screen_intro.h"
#include "ui_helpers.h"

// ---------------------------------------------------------------------------
// Static assets
// ---------------------------------------------------------------------------

const char* const IntroScreen::kText[IntroScreen::kCharCount] = {
    "k", "n", "o", "b", "b", "y", "."
};

const uint32_t IntroScreen::kColors[IntroScreen::kCharCount] = {
    0x9C5CFF, 0xF6C945, 0x42A5F5, 0x43A047, 0x43A047, 0xE53935, 0xFFFFFF
};

const lv_coord_t IntroScreen::kX[IntroScreen::kCharCount] = {
    56, 98, 140, 182, 214, 246, 284
};

// ---------------------------------------------------------------------------
// IntroScreen methods
// ---------------------------------------------------------------------------

void IntroScreen::create()
{
    screen_ = ui_create_base_screen();

    for (int i = 0; i < kCharCount; ++i) {
        lv_obj_t* parent = ui_get_safe_content();
        if (parent == nullptr) parent = screen_;
        letters_[i] = lv_label_create(parent);
        lv_label_set_text(letters_[i], kText[i]);
        lv_obj_set_style_text_color(letters_[i], lv_color_hex(kColors[i]), 0);
        lv_obj_set_style_text_font(letters_[i], &lv_font_montserrat_32, 0);
        // Position relative to center so layout adapts to safe content.
        lv_coord_t x_off = kX[i] - ui_safe_center_x();
        lv_coord_t y_off = 146 - ui_safe_center_y();
        lv_obj_align(letters_[i], LV_ALIGN_CENTER, x_off, y_off);
        lv_obj_add_flag(letters_[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void IntroScreen::refresh(uint8_t step)
{
    for (int i = 0; i < kCharCount; ++i) {
        if (letters_[i] == nullptr) continue;
        if (i < step) {
            lv_obj_clear_flag(letters_[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(letters_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

IntroScreen g_screen_intro;
