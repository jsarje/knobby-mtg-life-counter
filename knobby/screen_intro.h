// screen_intro.h
// Phase 5: IntroScreen class encapsulates the animated introduction screen.
// Owns the LVGL screen object and all letter labels; callers interact only
// through create() and refresh().

#pragma once

#include <lvgl.h>

class IntroScreen {
public:
    // Creates all LVGL objects.  Call once after LVGL is initialised.
    void create();

    // Updates label visibility to reveal letters up to step (0 = all hidden).
    void refresh(uint8_t step);

    // Returns the underlying LVGL screen object for lv_scr_load().
    lv_obj_t* lvObject() const { return screen_; }

private:
    static constexpr int kCharCount = 7;  // INTRO_CHAR_COUNT

    static const char* const kText[kCharCount];
    static const uint32_t    kColors[kCharCount];
    static const lv_coord_t  kX[kCharCount];

    lv_obj_t* screen_               = nullptr;
    lv_obj_t* letters_[kCharCount]  = {};
};

extern IntroScreen g_screen_intro;
