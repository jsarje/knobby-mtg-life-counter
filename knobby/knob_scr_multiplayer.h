#ifndef _KNOB_SCR_MULTIPLAYER_H
#define _KNOB_SCR_MULTIPLAYER_H

#include "knob_types.h"

// ---------- screens ----------
extern lv_obj_t *screen_multiplayer;
extern lv_obj_t *screen_multiplayer_menu;
extern lv_obj_t *screen_multiplayer_name;
extern lv_obj_t *screen_multiplayer_cmd_select;
extern lv_obj_t *screen_multiplayer_cmd_damage;
extern lv_obj_t *screen_multiplayer_all_damage;

// ---------- functions ----------
void build_multiplayer_screen(void);
void build_multiplayer_menu_screen(void);
void build_multiplayer_name_screen(void);
void build_multiplayer_cmd_select_screen(void);
void build_multiplayer_cmd_damage_screen(void);
void build_multiplayer_all_damage_screen(void);

void refresh_multiplayer_ui(void);
void refresh_multiplayer_menu_ui(void);
void refresh_multiplayer_name_ui(void);
void refresh_multiplayer_cmd_select_ui(void);
void refresh_multiplayer_cmd_damage_ui(void);
void refresh_multiplayer_all_damage_ui(void);

void open_multiplayer_screen(void);

#endif // _KNOB_SCR_MULTIPLAYER_H
