#ifndef _UI_MP_H
#define _UI_MP_H

#include "types.h"

// ---------- screens ----------
extern lv_obj_t *screen_4p;
extern lv_obj_t *screen_2p;
extern lv_obj_t *screen_3p;

// ---------- functions ----------
void build_multiplayer_screen(void);
void build_multiplayer_2p_screen(void);
void build_multiplayer_3p_screen(void);

void refresh_multiplayer_ui(void);

void open_multiplayer_screen(void);
void select_kick_timer(void);

#endif // _UI_MP_H
