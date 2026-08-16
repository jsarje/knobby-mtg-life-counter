#ifndef _DAMAGE_LOG_H
#define _DAMAGE_LOG_H

#include "types.h"

#define DAMAGE_LOG_MAX 256

typedef enum {
    LOG_EVT_LIFE = 0,
    LOG_EVT_CMD_DAMAGE,
    LOG_EVT_COUNTER,
} log_event_type_t;

/* Repurposes the source field carried by LOG_EVT_CMD_DAMAGE events
   (damage_log_entry_t.source and elimination_action_t.source): OR'd in
   when the damage came from the source player's *partner* commander.
   Safe because a partner-tagged source player is always 0..3
   (MAX_DISPLAY_PLAYERS - 1), well below this bit. */
#define CMD_DAMAGE_SOURCE_PARTNER_BIT 0x40

extern lv_obj_t *screen_damage_log;

void damage_log_add(int player, int delta, uint8_t event_type, int source);
void damage_log_reset(void);
void damage_log_remove_last_for(int player, uint8_t event_type);
void damage_log_select_next(void);
void damage_log_select_prev(void);
void damage_log_undo_selected(void);

void build_damage_log_screen(void);
void open_damage_log_screen(void);

#endif // _DAMAGE_LOG_H
