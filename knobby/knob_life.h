#ifndef _KNOB_LIFE_H
#define _KNOB_LIFE_H

#include "knob_types.h"

// ---------- state ----------
extern int life_total;
extern int pending_life_delta;
extern bool life_preview_active;
extern enemy_state_t enemies[ENEMY_COUNT];
extern int selected_enemy;
extern int multiplayer_life[MULTIPLAYER_COUNT];
extern int multiplayer_selected;
extern char multiplayer_names[MULTIPLAYER_COUNT][16];
extern int multiplayer_menu_player;
extern int multiplayer_cmd_source;
extern int multiplayer_cmd_target;
extern int multiplayer_cmd_delta;
extern int multiplayer_cmd_damage_totals[MULTIPLAYER_COUNT][MULTIPLAYER_COUNT];
extern int multiplayer_all_damage_value;
extern int multiplayer_pending_life_delta;
extern int multiplayer_preview_player;
extern bool multiplayer_life_preview_active;
extern int multiplayer_cmd_target_choices[MULTIPLAYER_COUNT - 1];
extern int dice_result;

// ---------- functions ----------
void knob_life_init(void);
void knob_life_reset(void);
void change_life(int delta);
void add_damage_to_selected_enemy(int delta);
void change_multiplayer_life(int delta);
void change_multiplayer_cmd_damage(int delta);
void change_multiplayer_all_damage(int delta);
void multiplayer_life_preview_commit_cb(lv_timer_t *timer);

// ---------- player colors ----------
lv_color_t get_player_base_color(int index);
lv_color_t get_player_active_color(int index);
lv_color_t get_player_text_color(int index);
lv_color_t get_player_preview_color(int index, int delta);
int get_main_player_index(void);
int get_cmd_target_player_index(int row);

#endif // _KNOB_LIFE_H
