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
