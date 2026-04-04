#pragma once

#include <lvgl.h>

// Create a standardized LVGL root screen with project defaults.
lv_obj_t* ui_create_base_screen();

// Create a standardized title label (white, montserrat_22, top-centered)
lv_obj_t* ui_create_title_label(lv_obj_t* parent, const char* text, lv_coord_t y_offset);

// Create a standardized action button with centered label and click callback.
lv_obj_t* ui_create_action_button(lv_obj_t* parent, const char* text,
                                  lv_coord_t width, lv_coord_t height,
                                  lv_event_cb_t cb);

// Composite layout for a large value with a secondary hint and title.
struct ValueHintLayout {
    lv_obj_t* title_label;
    lv_obj_t* value_label;
    lv_obj_t* hint_label;
};

// Create a set of title/value/hint labels positioned using the provided Y offsets.
ValueHintLayout ui_create_value_hint_layout(lv_obj_t* parent,
                                            const char* title_text,
                                            const char* hint_text,
                                            lv_coord_t title_y,
                                            lv_coord_t value_y,
                                            lv_coord_t hint_y);
