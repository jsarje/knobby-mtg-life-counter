// screen_multiplayer.cpp
// Phase 5: multiplayer screen class implementations.

#include "screen_multiplayer.h"
#include <stdio.h>

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

namespace {

struct SeatMetrics {
    bool       visible;
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    int16_t    rotation_deg;
    lv_coord_t content_pad_x;
    lv_coord_t content_pad_y;
};

struct LayoutMetrics {
    SeatMetrics seats[kMultiplayerCount];
};

LayoutMetrics build_layout(int active_player_count)
{
    LayoutMetrics layout = {};

    switch (active_player_count) {
    case 2:
        layout.seats[0] = {true, 0, 0, 360, 180, 180, 20, 18};
        layout.seats[1] = {true, 0, 180, 360, 180, 0, 20, 18};
        break;
    case 3:
        // Player 1 takes the full top half; players 2 and 3 split bottom half
        layout.seats[0] = {true, 0, 0, 360, 180, 180, 20, 16};
        layout.seats[1] = {true, 0, 180, 180, 180, 0, 16, 20};
        layout.seats[2] = {true, 180, 180, 180, 180, 0, 16, 20};
        break;
    case 4:
    default:
        layout.seats[0] = {true, 0, 0, 180, 180, 180, 18, 18};
        layout.seats[1] = {true, 180, 0, 180, 180, 180, 18, 18};
        layout.seats[2] = {true, 180, 180, 180, 180, 0, 18, 18};
        layout.seats[3] = {true, 0, 180, 180, 180, 0, 18, 18};
        break;
    }

    return layout;
}

lv_obj_t* make_button(lv_obj_t* parent, const char* txt,
                       lv_coord_t w, lv_coord_t h, lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    if (cb != nullptr) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, txt);
    lv_obj_center(label);

    return btn;
}

} // namespace

// ---------------------------------------------------------------------------
// MultiplayerScreen
// ---------------------------------------------------------------------------

lv_color_t MultiplayerScreen::baseColor(int index)
{
    static const uint32_t colors[kMultiplayerCount] = {
        0x7B1FE0, 0x29B6F6, 0xFFD600, 0xA5D6A7
    };
    if (index < 0 || index >= kMultiplayerCount) return lv_color_hex(0x303030);
    return lv_color_hex(colors[index]);
}

lv_color_t MultiplayerScreen::activeColor(int index)
{
    static const uint32_t colors[kMultiplayerCount] = {
        0x9C4DFF, 0x4FC3F7, 0xFFEA61, 0xC8E6C9
    };
    if (index < 0 || index >= kMultiplayerCount) return lv_color_hex(0x505050);
    return lv_color_hex(colors[index]);
}

lv_color_t MultiplayerScreen::textColor(int index)
{
    return (index == 2) ? lv_color_black() : lv_color_white();
}

lv_color_t MultiplayerScreen::previewColor(int index, int delta)
{
    if (index == 2) {
        return (delta < 0) ? lv_color_hex(0x7A1020) : lv_color_hex(0x215A2A);
    }
    return (delta < 0) ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN);
}

void MultiplayerScreen::create(lv_event_cb_t select_cb,
                                lv_event_cb_t open_menu_cb,
                                lv_event_cb_t swipe_pressed_cb,
                                lv_event_cb_t swipe_released_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);
    if (swipe_pressed_cb)  lv_obj_add_event_cb(screen_, swipe_pressed_cb,  LV_EVENT_PRESSED,  NULL);
    if (swipe_released_cb) lv_obj_add_event_cb(screen_, swipe_released_cb, LV_EVENT_RELEASED, NULL);

    for (int i = 0; i < kMultiplayerCount; ++i) {
        quadrants_[i] = lv_btn_create(screen_);
        lv_obj_set_style_radius(quadrants_[i], 0, 0);
        lv_obj_set_style_border_width(quadrants_[i], 1, 0);
        lv_obj_set_style_border_color(quadrants_[i], lv_color_black(), 0);
        lv_obj_set_style_shadow_width(quadrants_[i], 0, 0);
        lv_obj_set_style_pad_all(quadrants_[i], 0, 0);
        lv_obj_add_flag(quadrants_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        if (select_cb)    lv_obj_add_event_cb(quadrants_[i], select_cb,    LV_EVENT_CLICKED,      reinterpret_cast<void*>(static_cast<intptr_t>(i)));
        if (open_menu_cb) lv_obj_add_event_cb(quadrants_[i], open_menu_cb, LV_EVENT_LONG_PRESSED, reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        content_[i] = lv_obj_create(quadrants_[i]);
        lv_obj_set_style_bg_opa(content_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(content_[i], 0, 0);
        lv_obj_set_style_pad_all(content_[i], 0, 0);
        lv_obj_set_scrollbar_mode(content_[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(content_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(content_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(content_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_center(content_[i]);

        label_name_[i] = lv_label_create(content_[i]);
        lv_label_set_text(label_name_[i], "P?");
        lv_obj_set_style_text_color(label_name_[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(label_name_[i], &lv_font_montserrat_14, 0);

        label_life_[i] = lv_label_create(content_[i]);
        lv_label_set_text(label_life_[i], "40");
        lv_obj_set_style_text_color(label_life_[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(label_life_[i], &lv_font_montserrat_32, 0);

        badge_commander_[i] = lv_obj_create(content_[i]);
        lv_obj_set_size(badge_commander_[i], 28, 28);
        lv_obj_set_style_radius(badge_commander_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(badge_commander_[i], lv_color_hex(0xFFD600), 0);
        lv_obj_set_style_border_width(badge_commander_[i], 0, 0);
        lv_obj_set_scrollbar_mode(badge_commander_[i], LV_SCROLLBAR_MODE_OFF);
        lv_obj_clear_flag(badge_commander_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(badge_commander_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(badge_commander_[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_flag(badge_commander_[i], LV_OBJ_FLAG_HIDDEN);

        badge_commander_label_[i] = lv_label_create(badge_commander_[i]);
        lv_label_set_text(badge_commander_label_[i], "");
        lv_obj_set_style_text_font(badge_commander_label_[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(badge_commander_label_[i], lv_color_black(), 0);
        lv_obj_center(badge_commander_label_[i]);
    }
}

void MultiplayerScreen::refresh(const MultiplayerGameState& state)
{
    const LayoutMetrics layout = build_layout(state.active_player_count);
    char buf[8];

    for (int i = 0; i < kMultiplayerCount; ++i) {
        const bool active = state.isActivePlayerIndex(i) && layout.seats[i].visible;
        const bool preview_here = state.life_preview_active && (state.preview_player == i);
        const bool top_facing = (layout.seats[i].rotation_deg == 180);
        const lv_color_t txt_color = textColor(i);

        if (quadrants_[i] != nullptr) {
            if (!active) {
                lv_obj_add_flag(quadrants_[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(quadrants_[i], LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_pos(quadrants_[i], layout.seats[i].x, layout.seats[i].y);
                lv_obj_set_size(quadrants_[i], layout.seats[i].w, layout.seats[i].h);
            }

            lv_obj_set_style_bg_color(
                quadrants_[i],
                (i == state.selected) ? activeColor(i) : baseColor(i),
                0);
            lv_obj_set_style_bg_opa(quadrants_[i], LV_OPA_COVER, 0);
        }

        if (!active) {
            continue;
        }

        if (content_[i] != nullptr) {
            lv_obj_set_size(content_[i],
                            layout.seats[i].w - (layout.seats[i].content_pad_x * 2),
                            layout.seats[i].h - (layout.seats[i].content_pad_y * 2));
            lv_obj_center(content_[i]);
            lv_obj_set_style_transform_angle(content_[i], 0, 0);
        }

        if (label_life_[i] != nullptr) {
            if (preview_here) {
                snprintf(buf, sizeof(buf), "%+d", state.pending_life_delta);
                lv_label_set_text(label_life_[i], buf);
                lv_obj_set_style_text_color(label_life_[i], previewColor(i, state.pending_life_delta), 0);
            } else {
                snprintf(buf, sizeof(buf), "%d", state.life[i]);
                lv_label_set_text(label_life_[i], buf);
                lv_obj_set_style_text_color(label_life_[i], txt_color, 0);
            }
            lv_obj_set_style_transform_angle(label_life_[i], 0, 0);
            lv_obj_align(label_life_[i], LV_ALIGN_CENTER, 0, top_facing ? -10 : 12);
        }

        if (label_name_[i] != nullptr) {
            lv_label_set_text(label_name_[i], state.names[i]);
            lv_obj_set_style_text_color(label_name_[i], txt_color, 0);
            lv_obj_set_style_transform_angle(label_name_[i], 0, 0);
            if (top_facing) {
                lv_obj_align(label_name_[i], LV_ALIGN_BOTTOM_MID, 0, -8);
            } else {
                lv_obj_align(label_name_[i], LV_ALIGN_TOP_MID, 0, 8);
            }
        }

        if (badge_commander_[i] != nullptr && badge_commander_label_[i] != nullptr) {
            if (state.commander_tax[i] <= 0) {
                lv_obj_add_flag(badge_commander_[i], LV_OBJ_FLAG_HIDDEN);
            } else {
                char ctbuf[8];
                snprintf(ctbuf, sizeof(ctbuf), "%d", state.commander_tax[i]);
                lv_label_set_text(badge_commander_label_[i], ctbuf);
                lv_obj_set_style_transform_angle(badge_commander_[i], 0, 0);
                lv_obj_set_style_transform_angle(badge_commander_label_[i], 0, 0);
                if (top_facing) {
                    lv_obj_align(badge_commander_[i], LV_ALIGN_BOTTOM_LEFT, 8, -8);
                } else {
                    lv_obj_align(badge_commander_[i], LV_ALIGN_TOP_RIGHT, -8, 8);
                }
                lv_obj_clear_flag(badge_commander_[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// MultiplayerMenuScreen
// ---------------------------------------------------------------------------

void MultiplayerMenuScreen::create(lv_event_cb_t rename_cb,
                                    lv_event_cb_t cmd_damage_cb,
                                    lv_event_cb_t inc_commander_cb,
                                    lv_event_cb_t all_damage_cb,
                                    lv_event_cb_t players_cb,
                                    lv_event_cb_t settings_cb,
                                    lv_event_cb_t reset_cb,
                                    lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 26);

    btn_rename_    = make_button(screen_, "Rename",    180, 44, rename_cb);
    lv_obj_align(btn_rename_,    LV_ALIGN_CENTER, 0, -56);

    btn_cmd_damage_ = make_button(screen_, "Commander", 180, 44, cmd_damage_cb);
    lv_obj_align(btn_cmd_damage_, LV_ALIGN_CENTER, 0, -4);

    btn_inc_commander_ = make_button(screen_, "Tax", 180, 44, inc_commander_cb);
    lv_obj_align(btn_inc_commander_, LV_ALIGN_CENTER, 0, 48);

    btn_all_damage_ = make_button(screen_, "Global",    180, 44, all_damage_cb);
    lv_obj_align(btn_all_damage_, LV_ALIGN_CENTER, 0, -82);

    btn_players_ = make_button(screen_, "Players", 180, 44, players_cb);
    lv_obj_align(btn_players_, LV_ALIGN_CENTER, 0, -30);

    btn_menu_ = make_button(screen_, "Settings",  180, 44, settings_cb);
    lv_obj_align(btn_menu_, LV_ALIGN_CENTER, 0, 22);

    btn_reset_ = make_button(screen_, "Reset",     180, 44, reset_cb);
    lv_obj_align(btn_reset_, LV_ALIGN_CENTER, 0, 74);

    btn_back_ = make_button(screen_, "Back", 88, 42, back_cb);
    lv_obj_set_style_radius(btn_back_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(btn_back_, 0, 0);
    lv_obj_align(btn_back_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerMenuScreen::refresh(const MultiplayerGameState& state)
{
    char buf[32];
    const bool player_menu = (state.menu_mode == MULTIPLAYER_MENU_PLAYER);

    if (label_title_ != nullptr) {
        if (player_menu) {
            snprintf(buf, sizeof(buf), "%s Menu", state.names[state.menu_player]);
        } else {
            snprintf(buf, sizeof(buf), "Multiplayer");
        }
        lv_label_set_text(label_title_, buf);
    }

    auto show = [](lv_obj_t* w, bool visible) {
        if (w == nullptr) return;
        if (visible) lv_obj_clear_flag(w, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    };

    show(btn_rename_,    player_menu);
    show(btn_cmd_damage_, player_menu);
    show(btn_inc_commander_, player_menu);
    show(btn_all_damage_, !player_menu);
    show(btn_players_,    !player_menu);
    show(btn_menu_,       !player_menu);
    show(btn_reset_,      !player_menu);
    if (btn_back_ != nullptr) lv_obj_clear_flag(btn_back_, LV_OBJ_FLAG_HIDDEN);
}

// ---------------------------------------------------------------------------
// MultiplayerNameScreen
// ---------------------------------------------------------------------------

void MultiplayerNameScreen::create(lv_event_cb_t save_cb, lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 18);

    textarea_ = lv_textarea_create(screen_);
    lv_obj_set_size(textarea_, 240, 44);
    lv_obj_align(textarea_, LV_ALIGN_TOP_MID, 0, 56);
    lv_textarea_set_max_length(textarea_, 15);
    lv_textarea_set_one_line(textarea_, true);

    keyboard_ = lv_keyboard_create(screen_);
    lv_obj_set_size(keyboard_, 360, 170);
    lv_obj_align(keyboard_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(keyboard_, textarea_);

    lv_obj_t* btn_save = make_button(screen_, "Save", 88, 38, save_cb);
    lv_obj_align(btn_save, LV_ALIGN_TOP_RIGHT, -24, 116);

    lv_obj_t* btn_back = make_button(screen_, "Back", 88, 38, back_cb);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 24, 116);
}

void MultiplayerNameScreen::refresh(const MultiplayerGameState& state)
{
    char buf[40];
    if (label_title_ != nullptr) {
        snprintf(buf, sizeof(buf), "Rename %s", state.names[state.menu_player]);
        lv_label_set_text(label_title_, buf);
    }
    if (textarea_ != nullptr) {
        lv_textarea_set_text(textarea_, state.names[state.menu_player]);
    }
}

// ---------------------------------------------------------------------------
// MultiplayerCmdSelectScreen
// ---------------------------------------------------------------------------

void MultiplayerCmdSelectScreen::create(lv_event_cb_t target_pick_cb, lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 22);

    for (int i = 0; i < kMultiplayerCount - 1; ++i) {
        btn_target_[i] = lv_btn_create(screen_);
        lv_obj_set_size(btn_target_[i], 220, 46);
        lv_obj_align(btn_target_[i], LV_ALIGN_CENTER, 0, -42 + (i * 58));
        lv_obj_add_event_cb(btn_target_[i], target_pick_cb, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));

        label_target_[i] = lv_label_create(btn_target_[i]);
        lv_obj_set_style_text_font(label_target_[i], &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(label_target_[i], lv_color_white(), 0);
        lv_obj_center(label_target_[i]);
    }

    lv_obj_t* btn_back = make_button(screen_, "Back", 120, 46, back_cb);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerCmdSelectScreen::refresh(const MultiplayerGameState& state)
{
    char buf[48];
    if (!state.isActivePlayerIndex(state.menu_player)) return;

    if (label_title_ != nullptr) {
        snprintf(buf, sizeof(buf), "Source -> %s", state.names[state.menu_player]);
        lv_label_set_text(label_title_, buf);
    }

    int row = 0;
    for (int i = 0; i < state.active_player_count; ++i) {
        if (i == state.menu_player) continue;

        target_choices_[row] = i;

        if (btn_target_[row] != nullptr) {
            lv_obj_clear_flag(btn_target_[row], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(btn_target_[row],
                                       MultiplayerScreen::baseColor(i), 0);
            lv_obj_set_style_bg_opa(btn_target_[row], LV_OPA_COVER, 0);
        }
        if (label_target_[row] != nullptr) {
            snprintf(buf, sizeof(buf), "%s  %d",
                     state.names[i], state.cmd_damage_totals[i][state.menu_player]);
            lv_label_set_text(label_target_[row], buf);
            lv_obj_set_style_text_color(label_target_[row],
                                         MultiplayerScreen::textColor(i), 0);
        }
        ++row;
    }
    while (row < kMultiplayerCount - 1) {
        target_choices_[row] = -1;
        if (btn_target_[row] != nullptr) {
            lv_obj_add_flag(btn_target_[row], LV_OBJ_FLAG_HIDDEN);
        }
        ++row;
    }
}

// ---------------------------------------------------------------------------
// MultiplayerCmdDamageScreen
// ---------------------------------------------------------------------------

void MultiplayerCmdDamageScreen::create(lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 26);

    label_value_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_value_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_value_, &lv_font_montserrat_32, 0);
    lv_obj_align(label_value_, LV_ALIGN_CENTER, 0, -8);

    label_hint_ = lv_label_create(screen_);
    lv_label_set_text(label_hint_, "Turn knob for damage");
    lv_obj_set_style_text_color(label_hint_, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_hint_, &lv_font_montserrat_14, 0);
    lv_obj_align(label_hint_, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t* btn_back = make_button(screen_, "Back", 120, 46, back_cb);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerCmdDamageScreen::refresh(const MultiplayerGameState& state)
{
    char buf[48];
    if (!state.isActivePlayerIndex(state.cmd_target) ||
        !state.isActivePlayerIndex(state.cmd_source)) {
        return;
    }

    if (label_title_ != nullptr) {
        snprintf(buf, sizeof(buf), "%s -> %s",
                 state.names[state.cmd_source], state.names[state.cmd_target]);
        lv_label_set_text(label_title_, buf);
    }
    if (label_value_ != nullptr) {
        snprintf(buf, sizeof(buf), "Damage: %d",
                 state.cmd_damage_totals[state.cmd_source][state.cmd_target]);
        lv_label_set_text(label_value_, buf);
    }
}

// ---------------------------------------------------------------------------
// MultiplayerAllDamageScreen
// ---------------------------------------------------------------------------

void MultiplayerAllDamageScreen::create(lv_event_cb_t apply_cb, lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 26);

    label_value_ = lv_label_create(screen_);
    lv_obj_set_style_text_color(label_value_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_value_, &lv_font_montserrat_32, 0);
    lv_obj_align(label_value_, LV_ALIGN_CENTER, 0, -8);

    label_hint_ = lv_label_create(screen_);
    lv_label_set_text(label_hint_, "Turn knob, then apply");
    lv_obj_set_style_text_color(label_hint_, lv_color_hex(0x7A7A7A), 0);
    lv_obj_set_style_text_font(label_hint_, &lv_font_montserrat_14, 0);
    lv_obj_align(label_hint_, LV_ALIGN_CENTER, 0, 38);

    lv_obj_t* btn_apply = make_button(screen_, "Apply", 120, 46, apply_cb);
    lv_obj_align(btn_apply, LV_ALIGN_BOTTOM_MID, 0, -78);

    lv_obj_t* btn_back = make_button(screen_, "Back", 120, 46, back_cb);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerAllDamageScreen::refresh(const MultiplayerGameState& state)
{
    char buf[32];
    if (label_title_ != nullptr) {
        lv_label_set_text(label_title_, "All Players");
    }
    if (label_value_ != nullptr) {
        snprintf(buf, sizeof(buf), "Damage: %d", state.all_damage_value);
        lv_label_set_text(label_value_, buf);
    }
}

// ---------------------------------------------------------------------------
// MultiplayerPlayerCountScreen
// ---------------------------------------------------------------------------

void MultiplayerPlayerCountScreen::create(lv_event_cb_t select_two_cb,
                                          lv_event_cb_t select_three_cb,
                                          lv_event_cb_t select_four_cb,
                                          lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_label_set_text(label_title_, "Players");
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 26);

    btn_two_ = make_button(screen_, "2 Players", 180, 44, select_two_cb);
    lv_obj_align(btn_two_, LV_ALIGN_CENTER, 0, -56);

    btn_three_ = make_button(screen_, "3 Players", 180, 44, select_three_cb);
    lv_obj_align(btn_three_, LV_ALIGN_CENTER, 0, -4);

    btn_four_ = make_button(screen_, "4 Players", 180, 44, select_four_cb);
    lv_obj_align(btn_four_, LV_ALIGN_CENTER, 0, 48);

    lv_obj_t* btn_back = make_button(screen_, "Back", 88, 42, back_cb);
    lv_obj_set_style_radius(btn_back, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerPlayerCountScreen::refresh(const MultiplayerGameState& state)
{
    auto style_button = [this, &state](lv_obj_t* btn, int count) {
        if (btn == nullptr) return;
        const bool active = (state.active_player_count == count);
        lv_obj_set_style_bg_color(btn,
                                  active ? lv_color_hex(0x29B6F6) : lv_color_hex(0x3A3A3A),
                                  0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    };

    style_button(btn_two_, 2);
    style_button(btn_three_, 3);
    style_button(btn_four_, 4);
}

// ---------------------------------------------------------------------------
// MultiplayerPlayerCountConfirmScreen
// ---------------------------------------------------------------------------

void MultiplayerPlayerCountConfirmScreen::create(lv_event_cb_t confirm_cb, lv_event_cb_t back_cb)
{
    screen_ = lv_obj_create(NULL);
    lv_obj_set_size(screen_, 360, 360);
    lv_obj_set_style_bg_color(screen_, lv_color_black(), 0);
    lv_obj_set_style_border_width(screen_, 0, 0);
    lv_obj_set_scrollbar_mode(screen_, LV_SCROLLBAR_MODE_OFF);

    label_title_ = lv_label_create(screen_);
    lv_label_set_text(label_title_, "Change Player Count?");
    lv_obj_set_style_text_color(label_title_, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title_, &lv_font_montserrat_22, 0);
    lv_obj_align(label_title_, LV_ALIGN_TOP_MID, 0, 26);

    label_message_ = lv_label_create(screen_);
    lv_label_set_long_mode(label_message_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label_message_, 250);
    lv_obj_set_style_text_color(label_message_, lv_color_hex(0xD0D0D0), 0);
    lv_obj_set_style_text_align(label_message_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label_message_, LV_ALIGN_CENTER, 0, -12);

    lv_obj_t* btn_confirm = make_button(screen_, "Confirm", 140, 46, confirm_cb);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_MID, 0, -78);

    lv_obj_t* btn_back = make_button(screen_, "Back", 140, 46, back_cb);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void MultiplayerPlayerCountConfirmScreen::refresh(const MultiplayerGameState& state)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "Switch to %d players?\nThis resets the current game.",
             state.pending_player_count);
    lv_label_set_text(label_message_, buf);
}

// ---------------------------------------------------------------------------
// Global instances
// ---------------------------------------------------------------------------

MultiplayerScreen          g_screen_multiplayer;
MultiplayerMenuScreen      g_screen_multiplayer_menu;
MultiplayerNameScreen      g_screen_multiplayer_name;
MultiplayerCmdSelectScreen g_screen_multiplayer_cmd_select;
MultiplayerCmdDamageScreen g_screen_multiplayer_cmd_damage;
MultiplayerAllDamageScreen g_screen_multiplayer_all_damage;
MultiplayerPlayerCountScreen g_screen_multiplayer_player_count;
MultiplayerPlayerCountConfirmScreen g_screen_multiplayer_player_count_confirm;
