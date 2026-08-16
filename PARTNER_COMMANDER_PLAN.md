# Plan: Partner Commander Support

Add a per-tracked-player "Partner Commander" toggle (long-press on the Commander Damage tile)
that gives that player a second commander-damage source slot, and gates
`COUNTER_TYPE_PARTNER_TAX` per player. Damage rows are computed dynamically per target so a
4-tracked-player game with all 3 opponents partnered shows exactly 6 rows.

## Confirmed decisions (from user)
- Toggle UI: long-press "Commander Damage" tile in the player menu (checkbox + Apply screen,
  same convention as build_all_damage_screen's cb_include_myself).
- Partner row label: just "P2" (no text suffix) — distinguish primary vs partner rows by
  reusing the existing mana icons (MANA_ICON_COMMANDER for primary, MANA_ICON_PARTY for partner)
  as a small badge on the row, matching counter_definitions icons already used for
  Commander Tax / Partner Tax.
- Partner flag must ride in Table Sync (all devices must agree on row layout) — bump
  KNOBBY_NET_VERSION 1 -> 2. Accepted tradeoff: old/new firmware won't sync until all updated.

## Data model
Only tracked players 0..MAX_DISPLAY_PLAYERS-1 can have a partner; untracked extra opponents
4..7 in solo/"track fewer than num" modes never get partner rows.

- `knobby/src/game.h` / `game.c`: new `bool player_has_partner[MAX_DISPLAY_PLAYERS]` (RAM-only,
  same tier/reset behavior as `player_has_override` — NOT persisted to NVS, resets on reboot,
  survives `knob_life_reset()` same as player colors do today).
- New `int cmd_damage_partner_totals[MAX_DISPLAY_PLAYERS][MAX_DISPLAY_PLAYERS]` parallel to
  existing `cmd_damage_totals[MAX_GAME_PLAYERS][MAX_DISPLAY_PLAYERS]` (source x target), storing
  damage dealt by each tracked source's *partner* commander. Only ever written when
  `player_has_partner[source]` true, but always safe to read (0 if unused).
- `knobby/src/types.h`: new `#define MAX_CMD_DAMAGE_ROWS (MAX_ENEMY_COUNT + MAX_DISPLAY_PLAYERS - 1)`
  (7 + 3 = 10) — worst case: up to 7 opponent slots (solo-vs-many mode) plus up to 3 extra partner
  rows from tracked opponents. The common 4-tracked-all-partnered case yields exactly 6.
- Row mapping: replace the stateless `get_cmd_target_player_index(row)` arithmetic with a cache
  built once per `prepare_cmd_damage_for_player(target)` call: iterate players 0..num-1 (skipping
  target), emit 1 row per player, plus 1 extra row immediately after if that player is tracked
  and `player_has_partner[i]` is true. Store `{int8_t player; bool is_partner;}` per row in a
  static array in game.c. Keep `get_cmd_target_player_index(row)` as the accessor (reads cache,
  falls back to today's "assume target=player 0, no partner rows" degenerate path when
  `cmd_damage_target < 0`, matching the existing documented fallback). Add new
  `bool get_cmd_row_is_partner(int row)` accessor.
- `enemies[]`, `select_rows[]`, `label_enemy_name[]`, `label_enemy_damage[]` (ui_1p.c) resized
  from `MAX_ENEMY_COUNT` to `MAX_CMD_DAMAGE_ROWS`.
- `damage_apply()` (game.c): route the write to `cmd_damage_partner_totals[source][target]`
  instead of `cmd_damage_totals[source][target]` when `get_cmd_row_is_partner(selected_enemy)`.
- `undo_cmd_damage(source, target, delta)`: needs a partner flag too. Encode it in the existing
  reused `source` field instead of widening structs (matches the codebase's existing convention
  of repurposing that field per event type):
  - `damage_log_entry_t.source` (int8_t, damage_log.c) — OR in bit 0x40 when partner
    (`(int8_t)(source_player | 0x40)`); decode in `damage_log.c`'s undo dispatch and in
    `undo_elimination_action`. Safe because tracked source_player is always 0..3.
  - `elimination_action_t.source` (int, game.c) — same 0x40 bit trick.
  - `undo_cmd_damage` gains an `is_partner` bool derived from decoding that bit at each call site
    (damage_log.c line ~114, game.c undo_elimination_action line ~154).
- `check_player_elimination()` 21-damage scan: also scan `cmd_damage_partner_totals[i][player]`
  for `i` in `0..MAX_DISPLAY_PLAYERS-1` (per-commander independent threshold — this matches
  actual MTG rules, partner damage does NOT combine with primary for the 21 total).
- `manual_uneliminate_player()`'s cmd-damage clamp-to-20 loop: mirror for the partner array.
- `knob_life_reset()` / `knob_life_init()`: `memset(cmd_damage_partner_totals, 0, ...)` alongside
  the existing `cmd_damage_totals` reset. Do NOT reset `player_has_partner[]` here (same
  session-persists-until-reboot tier as player colors).

## Phase 2 — Table Sync protocol (depends on Phase 1)
- `knobby/src/net_sync.h`: add `uint8_t cmd_damage_partner[NET_SYNC_MAX_PLAYERS]` to
  `net_sync_player_t` (parallel column to `cmd_damage[]`, but only 4 slots since only tracked
  players can be partner sources). Repurpose the existing `reserved` byte as `flags`; bit 0
  = `NET_SYNC_HAS_PARTNER` (whether this player block's owner has partner enabled).
- `knobby/src/game.c`: `net_sync_fill_state()` / `net_sync_apply_state()` — fill/adopt
  `cmd_damage_partner[]` and the `HAS_PARTNER` flag alongside existing fields. Toggling partner
  on/off for a player must call `net_sync_commit_player()` (bump that player's version) so the
  change propagates like any other player-block edit.
- `knobby/knobby_net.cpp`: bump `KNOBBY_NET_VERSION` from 1 to 2 (line ~35).

## Phase 3 — Counter gating (depends on Phase 1, parallel with Phase 2)
- `knobby/src/game.c` / `game.h`: new `bool counter_type_available_for_player(int player,
  counter_type_t type)` = `counter_type_is_enabled(type) && (type != COUNTER_TYPE_PARTNER_TAX ||
  player_has_partner[player])`.
- Swap this in for `counter_type_is_enabled()` calls that are per-player:
  - `refresh_1p_counters()` in `knobby/src/ui_1p.c` (line ~139).
  - `refresh_counter_rows()` in `knobby/src/ui_mp.c` (line ~375) — `player_index` param already
    available.
- `knobby/src/ui_player_menu.c`: new `refresh_counter_menu_ui()` — greys out/disables the Partner
  Tax tile (`lv_obj_get_child(screen_counter_menu, 1)`, its icon + label children) using the same
  visual treatment `build_quad_screen` applies to disabled tiles (bg `0x111111`/60% opa, text
  `0x555555`, `LV_OBJ_FLAG_CLICKABLE` cleared) vs enabled (`0x1A1A2E`, white text, clickable).
  Call it from `open_counter_menu()`. Guard `event_counter_partner_tax` to no-op when
  `!player_has_partner[menu_player]`.

## Phase 4 — Partner Commander toggle UI (depends on Phase 1)
- `knobby/src/ui_player_menu.c` / `.h`: new `screen_player_partner_menu` (or similar) — checkbox
  + Apply screen, built the same way as `build_all_damage_screen`'s `cb_include_myself`
  (checkbox styled with larger hit target, title shows `player_names[menu_player]`).
- Wire via long-press: in `build_player_menu_screen()`, add
  `lv_obj_add_event_cb(cmd_damage_btn, event_menu_partner_toggle, LV_EVENT_LONG_PRESSED, NULL)`
  on the "Commander Damage" tile (`lv_obj_get_child(screen_player_menu, 1)`), same pattern as
  the existing Rename-tile and Counters-tile long-press wiring.
- On Apply: set `player_has_partner[menu_player]`, call `net_sync_commit_player(menu_player)`,
  refresh counter menu gating + multiplayer/1p counter rows, `back_to_main()`.
- `knobby/knob.c`: add the new screen's `build_*` call to `knob_gui()` alongside the other
  `build_*` calls (~line 430).

## Phase 5 — Commander-damage select/damage screen UI (depends on Phase 1, parallel with 2-4)
- `knobby/src/ui_1p.c`:
  - Resize `select_rows[]`, `label_enemy_name[]`, `label_enemy_damage[]`, `enemies[]` to
    `MAX_CMD_DAMAGE_ROWS`; update the creation loop in `build_select_screen()` and the hide loop
    in `refresh_select_ui()` from `MAX_ENEMY_COUNT` to `MAX_CMD_DAMAGE_ROWS`.
  - `style_select_entry(i, player_index)`: add a small icon badge using `get_cmd_row_is_partner(i)`
    to pick `MANA_ICON_COMMANDER` vs `MANA_ICON_PARTY` (same icons/font as
    `counter_definitions[COUNTER_TYPE_COMMANDER_TAX/PARTNER_TAX]`), rendered small in a corner of
    the row box. Player name stays just "P2" (no text suffix) per decision above.
  - `refresh_damage_ui()` / `open_damage_screen()`: same icon badge on the per-row damage entry
    screen next to `label_damage_title`.
  - The existing dynamic grid math in `refresh_select_ui()` (2 cols, computed rows/height) already
    generalizes to more rows without changes; verify visually it stays legible up to 6 rows (the
    guaranteed common case) — rows beyond 6 (rare combined solo+partner edge case) may look
    cramped, accepted as out of scope for now.

## Relevant files
- `knobby/src/types.h` — `MAX_CMD_DAMAGE_ROWS`.
- `knobby/src/game.h`, `knobby/src/game.c` — data model, row cache, elimination, net_sync fill/apply,
  counter gating.
- `knobby/src/damage_log.c` — decode partner bit in undo dispatch.
- `knobby/src/net_sync.h` — `net_sync_player_t` new field + flags bit.
- `knobby/knobby_net.cpp` — version bump.
- `knobby/src/ui_1p.c` — select-screen arrays/layout/labels/icons, damage screen.
- `knobby/src/ui_mp.c` — counter row gating.
- `knobby/src/ui_player_menu.c`, `.h` — counter menu gating, new toggle screen + long-press wiring.
- `knobby/knob.c` — register new screen builder in `knob_gui()`.

## Verification
1. `arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio" knobby`
2. sim build (sim-web.sh / sim/Makefile) — shared `knobby/src/*.c` must also compile there.
3. Manual: 4p game, enable partner on 2-3 players, confirm 6-slot grid on a target facing all-
   partnered opponents; Partner Tax hidden/disabled for non-partner players in the counter menu
   and on-panel, editable for partner players; Table Sync agreement between two devices (toggle
   partner on one device, confirm the other device's row count updates); undo from damage log for
   both a primary and a partner commander-damage entry restores the correct array; elimination at
   21 fires from partner damage alone.

## Scope boundaries
- Partner applies only to the 4 tracked/displayed players — untracked extra opponents (5th-8th,
  solo-vs-many mode) never get a partner toggle or partner rows.
- No new persisted (NVS) setting — matches existing player-color-override precedent (RAM-only,
  resets on power cycle, survives in-session game reset).
- 21-damage lethal threshold applies per-commander independently (primary and partner tracked
  separately), matching actual MTG rules — not summed.
- Not fixing select-screen grid density for the rare >6-row combined solo+partner edge case.
