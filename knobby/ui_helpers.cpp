#include "ui_helpers.h"
#include "pincfg.h"
#include <algorithm>
#include <cmath>

static lv_obj_t* g_safe_content = nullptr;
static lv_coord_t g_screen_w = 360;
static lv_coord_t g_screen_h = 360;
static lv_coord_t g_center_x = 180;
static lv_coord_t g_center_y = 180;
static lv_coord_t g_safe_radius = 160;

static void ui_compute_metrics()
{
    g_center_x = g_screen_w / 2;
    g_center_y = g_screen_h / 2;
    g_safe_radius = std::min(g_center_x, g_center_y) - UI_SAFE_INSET_PX;
}

void ui_init_safe_area(lv_obj_t* parent)
{
    if (parent == nullptr) return;

    // get display resolution if LVGL exposes it
    lv_disp_t* disp = lv_disp_get_default();
    if (disp) {
        g_screen_w = lv_disp_get_hor_res(disp);
        g_screen_h = lv_disp_get_ver_res(disp);
    }
    ui_compute_metrics();

    if (g_safe_content != nullptr) {
        // already initialized for this parent: keep it. If parent changed,
        // delete and recreate under the new parent (LVGL lacks a simple reparent API).
        if (lv_obj_get_parent(g_safe_content) == parent) return;
        lv_obj_del(g_safe_content);
        g_safe_content = nullptr;
    }

    g_safe_content = lv_obj_create(parent);
    lv_coord_t w = g_screen_w - 2 * UI_SAFE_INSET_PX;
    lv_coord_t h = g_screen_h - 2 * UI_SAFE_INSET_PX;
    lv_obj_set_size(g_safe_content, w, h);
    lv_obj_center(g_safe_content);
    lv_obj_set_style_pad_all(g_safe_content, 0, 0);
    lv_obj_clear_flag(g_safe_content, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* ui_get_safe_content()
{
    return g_safe_content;
}

lv_coord_t ui_safe_center_x() { return g_center_x; }
lv_coord_t ui_safe_center_y() { return g_center_y; }
lv_coord_t ui_safe_radius() { return g_safe_radius; }
lv_coord_t ui_safe_width() { return (g_screen_w - 2 * UI_SAFE_INSET_PX); }

lv_obj_t* ui_create_base_screen()
{
    lv_obj_t* scr = lv_obj_create(NULL);
    // Use display resolution when available
    lv_disp_t* disp = lv_disp_get_default();
    if (disp) {
        g_screen_w = lv_disp_get_hor_res(disp);
        g_screen_h = lv_disp_get_ver_res(disp);
        lv_obj_set_size(scr, g_screen_w, g_screen_h);
    } else {
        lv_obj_set_size(scr, g_screen_w, g_screen_h);
    }
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    // Note: safe-area initialization is done on-demand by screens to avoid
    // reparent/recreate thrash that can cause display flicker.
    g_safe_content = nullptr;
    return scr;
}

lv_obj_t* ui_create_title_label(lv_obj_t* parent, const char* text, lv_coord_t y_offset)
{
    if (parent == nullptr) parent = ui_get_safe_content();
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
    if (parent == nullptr) parent = ui_get_safe_content();
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

ValueHintLayout ui_create_value_hint_layout(lv_obj_t* parent,
                                            const char* title_text,
                                            const char* hint_text,
                                            lv_coord_t title_y,
                                            lv_coord_t value_y,
                                            lv_coord_t hint_y)
{
    ValueHintLayout out = {nullptr, nullptr, nullptr};

    out.title_label = ui_create_title_label(parent, title_text, title_y);

    out.value_label = lv_label_create(parent);
    lv_obj_set_style_text_color(out.value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(out.value_label, &lv_font_montserrat_32, 0);
    lv_obj_align(out.value_label, LV_ALIGN_CENTER, 0, value_y);

    out.hint_label = lv_label_create(parent);
    if (hint_text != nullptr) lv_label_set_text(out.hint_label, hint_text);
    lv_obj_set_style_text_color(out.hint_label, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(out.hint_label, &lv_font_montserrat_14, 0);
    lv_obj_align(out.hint_label, LV_ALIGN_CENTER, 0, hint_y);

    return out;
}
