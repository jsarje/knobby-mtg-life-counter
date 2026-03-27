---
title: Commander Tax Tracking (Design)
version: 1.0
date_created: 2026-03-27
last_updated: 2026-03-27
tags: [design, multiplayer, ui, gameplay]
---

# Introduction

This specification defines requirements, interfaces, UI behavior, data contracts, and acceptance criteria for tracking each player's "commander tax" in the Knobby multiplayer life-counter firmware.

Commander tax is an integer value that starts at 0 for every player and can be incremented from a player's long-press menu. The current commander tax should be shown concisely on each player's quadrant.

## 1. Purpose & Scope

Purpose: Add clear, testable behavior and interface definitions so implementers and generative AIs can add commander tax support reliably.

Scope: UI and data model changes to support the following features:
- A new long-press menu item for a player: "Increment Commander Tax" (increment by 1).
- A concise commander-tax indicator displayed on each player's quadrant showing the current value.
- Persistence of commander-tax within the active session and synchronization for multiplayer mode.

Intended audience: firmware engineers, UI designers, and test engineers working on the firmware.

Assumptions:
- Players are represented by a player object/struct in `session_state` or equivalent.
- The UI supports per-quadrant overlays and long-press context menus.

## 2. Definitions

- Commander Tax (CT): Non-negative integer representing extra points a player must pay; starts at 0.
- Quadrant: The region of the screen dedicated to a single player.
- Long-press menu: Context menu activated by a long press on a player's quadrant.
- Session state: In-memory representation of the current game and player states (see [knobby/session_state.cpp](knobby/session_state.cpp#L1)).

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: Provide a long-press menu item labelled `Increment Commander Tax` for each player.
- **REQ-002**: Selecting `Increment Commander Tax` increments that player's commander tax by exactly 1.
- **REQ-003**: Each player's quadrant must display a concise commander-tax indicator showing the current integer value.
- **REQ-004**: Commander tax initial value must be 0 when a new session starts or when a new player is created.
- **REQ-005**: Commander tax changes must update the UI immediately (no manual refresh required).
- **REQ-006**: Commander tax must be persisted in the session state and included in any session serialization used for local save/restore.
- **REQ-007**: In multiplayer mode, changes to a player's commander tax must be propagated to remote players (synchronized state update).
- **REQ-008**: Commander tax must be a non-negative integer; no automatic decrements or negative values through the UI.
- **CON-001**: Keep UI indicator concise: use a small badge or `CT: N` label that does not obscure the quadrant's primary content.
- **GUD-001**: If commander tax is 0, the UI may either show `CT: 0` or hide the badge; choose the approach consistent with existing quadrant badges.

## 4. Interfaces & Data Contracts

Data model (C++-like pseudocode to be adopted in `session_state`):

```cpp
struct PlayerState {
    // existing player fields...
    int commander_tax = 0; // REQ-004 - default 0
};

struct SessionState {
    // existing session fields...
    std::vector<PlayerState> players;
};
```

UI contract:
- Long-press menu entry: { id: "inc_commander_tax", label: "Increment Commander Tax" }
- When selected, the UI layer issues an action/event: `action_increment_commander_tax(player_id)`.

Event/API behavior (synchronous contract):
- `action_increment_commander_tax(player_id)`:
  - Reads `session_state.players[player_id].commander_tax`.
  - Increments by 1.
  - Persists the updated `session_state` and triggers UI redraw for the affected quadrant.
  - Emits a multiplayer sync message if in multiplayer mode: `{ type: "player_update", player_id: X, commander_tax: N }`.

Multiplayer sync message schema (JSON example):

```json
{
  "type": "player_update",
  "player_id": 2,
  "commander_tax": 3
}
```

UI display guidance:
- Badge layout: a small rounded rectangle near the quadrant corner showing `CT: N` or just `N`.
- Font size: small but legible given screen density; do not overlap critical information like life total.

Files likely to change (implementation pointers):
- [knobby/session_state.cpp](knobby/session_state.cpp#L1) and [knobby/session_state.h](knobby/session_state.h#L1) — add `commander_tax` to player struct.
- [knobby/screen_multiplayer.cpp](knobby/screen_multiplayer.cpp#L1) and [knobby/screen_multiplayer.h](knobby/screen_multiplayer.h#L1) — ensure sync message handling includes `commander_tax`.
- [knobby/scr_st77916.cpp](knobby/scr_st77916.cpp#L1) or quadrant rendering code — draw the badge.
- [knobby/navigation_controller.cpp](knobby/navigation_controller.cpp#L1) or `long-press` handler — add menu entry and action dispatch.

## 5. Acceptance Criteria

- **AC-001**: Given a player's quadrant is long-pressed, When the long-press menu opens, Then the menu contains an `Increment Commander Tax` entry.
- **AC-002**: Given the menu entry is selected, When the action completes, Then the player's commander tax value increases by 1 and the quadrant display shows the new value.
- **AC-003**: Given a fresh session, When players are created, Then each player has `commander_tax == 0`.
- **AC-004**: Given a commander tax update in multiplayer, When the originating device increments a player's commander tax, Then all connected devices reflect the updated value within the same multiplayer update cycle.
- **AC-005**: Given session save/restore, When the session is restored, Then each player's commander tax matches the saved value.

## 6. Test Automation Strategy

- Test Levels: Unit, Integration, End-to-End (device).

- Unit tests:
  - Add tests for `PlayerState` default initialization (`commander_tax == 0`).
  - Test `increment_commander_tax(player_id)` logic updates state and returns expected value.

- Integration tests:
  - Simulate UI action dispatch to `action_increment_commander_tax` and assert UI redraw call for the affected quadrant.
  - Simulate session serialization and restore to verify `commander_tax` persisted.

- End-to-end / Manual device tests:
  - Long-press a quadrant, select menu action, verify visual badge increments.
  - On two devices connected in multiplayer, increment on device A and verify device B receives update.

- CI Integration:
  - Run unit and integration tests in the existing CI pipeline.

- Coverage Requirements:
  - Unit tests should cover increment logic and session (de)serialization paths; no specific global coverage threshold mandated for this small feature.

## 7. Rationale & Context

Commander tax is a common mechanic in Commander-format gameplay; tracking it directly in the UI avoids manual note-taking and reduces game friction. Increment-only via the long-press menu keeps the action simple and avoids accidental bulk edits.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: Multiplayer synchronization service — used to broadcast `player_update` messages in existing multiplayer code paths.

### Third-Party Services
- **SVC-001**: None required beyond existing networking components.

### Infrastructure Dependencies
- **INF-001**: N/A (feature lives within firmware runtime and existing persistence mechanisms).

### Data Dependencies
- **DAT-001**: Session serialization format — ensure `commander_tax` is included when saving or sending session-state payloads.

### Technology Platform Dependencies
- **PLT-001**: LVGL UI layer for rendering quadrant overlays; code must follow LVGL-friendly redraw patterns.

### Compliance Dependencies
- **COM-001**: None.

## 9. Examples & Edge Cases

Example: increment action sequence

```cpp
// UI layer
on_long_press(player_id) {
  show_menu(player_id, [ { id: "inc_commander_tax", label: "Increment Commander Tax" } ]);
}

// Menu selection handler
if (menu_item.id == "inc_commander_tax") {
  action_increment_commander_tax(player_id);
}

// Action implementation
void action_increment_commander_tax(int player_id) {
  session.players[player_id].commander_tax += 1;
  persist_session_state();
  redraw_quadrant(player_id);
  if (multiplayer_active()) send_player_update(player_id);
}
```

Edge cases:
- If a menu action is received for a non-existent player_id, ignore and log the event (no crash).
- If a multiplayer update arrives with a negative `commander_tax`, reject and log error.
- Large values: commander_tax should remain an `int`; but UI may cap displayed digits or use compact display if number grows large.

## 10. Validation Criteria

- Unit tests and integration tests pass in CI covering increment and persistence paths.
- Manual device tests demonstrate UI correctness and multiplayer synchronization.

## 11. Related Specifications / Further Reading

- [spec-architecture-cpp-oop-refactor-plan.md](spec/spec-architecture-cpp-oop-refactor-plan.md)
- Firmware README: [README.md](README.md#L1)

---

Implementation notes (short):
- Add an `int commander_tax` field to the player model in `session_state` and include it in session (de)serialization.
- Add `inc_commander_tax` id and handler into the long-press menu code (likely `navigation_controller` / `bidi_switch_knob` handlers).
- Render a small `CT` badge in the quadrant rendering code (follow existing badge rendering convention).
- Ensure multiplayer message handlers apply updates to `commander_tax` and trigger redraws.
