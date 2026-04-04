#include "ui_helpers.h"

lv_obj_t* ui_create_base_screen()
{
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_size(scr, 360, 360);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    return scr;
}

lv_obj_t* ui_create_title_label(lv_obj_t* parent, const char* text, lv_coord_t y_offset)
{
    lv_obj_t* label = lv_label_create(parent);
    if (text != nullptr) lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, y_offset);
    return label;
}

lv_obj_t* ui_create_action_button(lv_obj_t* parent, const char* text,
                                  lv_coord_t width, lv_coord_t height,
                                  lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    if (cb != nullptr) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t* label = lv_label_create(btn);
    if (text != nullptr) lv_label_set_text(label, text);
    lv_obj_center(label);
    return btn;
}
