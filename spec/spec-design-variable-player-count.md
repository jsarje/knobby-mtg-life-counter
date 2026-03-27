---
title: Variable Player Count and Adaptive Multiplayer Layout (Design)
version: 1.0
date_created: 2026-03-27
last_updated: 2026-03-27
owner: Knobby MTG Life Counter Team
tags: [design, multiplayer, ui, layout, session]
---

# Introduction

This specification defines how the multiplayer firmware shall support a runtime-configurable player count of 2, 3, or 4 players while keeping 4 players as the default on boot. It also defines an adaptive overview layout that scales from halves to thirds to quarters on the round display without requiring persistent storage of the selected player count.

## 1. Purpose & Scope

Purpose: define the gameplay, state, and UI contracts required to support a variable active player count in the multiplayer overview and related flows.

Scope:
- Session-state changes required to track an active player count between 2 and 4.
- UI and navigation changes required to let the user change player count during runtime.
- Overview layout behavior for 2-player, 3-player, and 4-player games.
- Rules for how existing multiplayer features behave when fewer than 4 players are active.

Intended audience: firmware engineers, UI implementers, and test engineers working on the multiplayer experience.

Assumptions:
- The hardware display remains a 360 x 360 round panel.
- Existing multiplayer features remain in scope: life tracking, commander damage, commander tax, rename, global damage, reset, and settings.
- Player count is a session concern, not a device setting.

## 2. Definitions

- Active Player Count (APC): the number of currently active seats in the session. Valid values are 2, 3, and 4.
- Seat: one active player slot in the multiplayer session.
- Seat Segment: the screen region assigned to one active player in the multiplayer overview.
- Radial Layout: a layout where seat regions are distributed around the circular display by angle.
- Overview Screen: the default multiplayer gameplay screen that shows all active players.
- Count Selector: the UI flow that allows choosing 2, 3, or 4 active players.
- Dirty Session: a session whose gameplay state differs from new-game defaults for the current APC.

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: The multiplayer session shall support an active player count of 2, 3, or 4.
- **REQ-002**: The firmware shall default to 4 active players on boot.
- **REQ-003**: The selected active player count shall not be persisted across power cycles, reboots, or firmware restarts.
- **REQ-004**: The user shall be able to change the active player count from the multiplayer global menu.
- **REQ-005**: The player-count selector shall offer exactly three choices: `2 Players`, `3 Players`, and `4 Players`.
- **REQ-006**: Changing the active player count shall reset gameplay session data that depends on seat count, including life totals, commander tax, commander damage totals, selected player, pending preview state, and default player names.
- **REQ-007**: The overview screen shall render one seat segment per active player and shall not show inactive player slots.
- **REQ-008**: The overview layout shall use equal angular segmentation of the circular display: 180 degrees per seat for 2 players, 120 degrees per seat for 3 players, and 90 degrees per seat for 4 players.
- **REQ-009**: Each seat segment shall display the assigned player's life total as the primary value, the player name as secondary text, and commander tax when greater than 0.
- **REQ-010**: Text and numeric content in each seat segment shall be oriented toward that player's edge of the display by rotating an inner content container, not by rotating the touch target itself.
- **REQ-011**: The currently selected active player shall remain visually highlighted in every supported layout.
- **REQ-012**: Touch selection, long-press player menu entry, and knob-driven life changes shall operate only on active players.
- **REQ-013**: Commander-damage source selection shall list only active players other than the current target player.
- **REQ-014**: Global damage application shall affect only active players.
- **REQ-015**: Rename, commander tax, and commander damage actions shall be unavailable for inactive player slots.
- **REQ-016**: If the user selects the currently active player count, the firmware shall perform no reset and shall leave the current session unchanged.
- **REQ-017**: If the user attempts to change active player count while the session is dirty, the firmware shall require explicit confirmation before resetting the session.
- **REQ-018**: A standard multiplayer reset shall preserve the current active player count for the ongoing runtime session.
- **REQ-019**: All gameplay indices and iteration over players shall be bounded by the active player count, not by the maximum supported player capacity.
- **REQ-020**: The implementation shall retain a maximum supported player capacity of 4 to minimize structural changes in existing arrays and controller logic.
- **CON-001**: The design shall remain usable on a 360 x 360 round display without requiring scrolling on the overview screen.
- **CON-002**: The design shall not introduce any requirement for non-volatile storage.
- **CON-003**: Inactive player slots shall not influence visible totals, menus, hit-testing, or commander-damage calculations.
- **CON-004**: The overview layout shall reserve a neutral center area for visual breathing room and future status affordances; player content shall not be packed into the exact screen center.
- **GUD-001**: The count selector should live in the multiplayer global menu rather than the settings screen because player count is part of game setup, not device configuration.
- **GUD-002**: Hidden player slots should be reset to new-game defaults whenever APC changes so that expanding from a lower count back to a higher count yields predictable state.
- **GUD-003**: The selected player's highlight treatment should remain color-based and consistent with the current multiplayer screen language.
- **PAT-001**: Use a template-driven radial layout planner that derives segment geometry from APC instead of maintaining separate hard-coded screens for 2, 3, and 4 players.
- **PAT-002**: Keep fixed-size backing arrays sized to 4 and add an `active_player_count` field to the gameplay state.

## 4. Interfaces & Data Contracts

### 4.1 Gameplay State Contract

The gameplay state shall continue to support a maximum capacity of 4 players while adding an explicit active count.

```cpp
static constexpr int kMaxPlayers = 4;
static constexpr int kDefaultActivePlayerCount = 4;
static constexpr int kMinActivePlayerCount = 2;

struct MultiplayerGameState {
    int active_player_count = kDefaultActivePlayerCount;
    int life[kMaxPlayers];
    int commander_tax[kMaxPlayers];
    int cmd_damage_totals[kMaxPlayers][kMaxPlayers];
    char names[kMaxPlayers][kPlayerNameMaxLen];

    int selected = 0;
    int menu_player = 0;
    int cmd_source = 0;
    int cmd_target = -1;
    int preview_player = -1;
    int pending_life_delta = 0;
    bool life_preview_active = false;
};
```

State rules:
- Valid active player indices are `0 .. active_player_count - 1`.
- Slots `active_player_count .. 3` are inactive and shall be ignored by gameplay logic.
- When APC changes, all four backing slots shall be reset, then the first `active_player_count` slots become active.

### 4.2 Layout Contract

The overview screen shall derive seat regions from a layout planner.

```cpp
struct SeatLayoutRegion {
    uint8_t player_index;
    int16_t start_angle_deg;
    int16_t sweep_angle_deg;
    int16_t content_rotation_deg;
    lv_area_t touch_bounds;
};

struct MultiplayerLayoutPlan {
    uint8_t active_player_count;
    SeatLayoutRegion seats[kMaxPlayers];
    uint16_t center_deadzone_diameter;
};
```

Layout rules:
- For APC `2`, the layout planner shall create two opposing 180-degree seat segments.
- For APC `3`, the layout planner shall create three evenly spaced 120-degree seat segments.
- For APC `4`, the layout planner shall create four evenly spaced 90-degree seat segments.
- Seat order shall proceed clockwise from the top seat index `0`.
- Each segment shall own one touch target and one separately rotatable content container.

### 4.3 Navigation Contract

The multiplayer global menu shall add a new entry:

```text
Players
```

Selecting `Players` shall open a count selector screen with:

```text
2 Players
3 Players
4 Players
Back
```

If the chosen count differs from the current APC:
- If the session is clean, apply the APC change immediately.
- If the session is dirty, open a confirmation step before applying the reset.

Confirmation contract:

```text
Change player count?
This resets the current game.
Confirm
Back
```

### 4.4 Controller/API Behavior

Required controller operations:

```cpp
void setActivePlayerCount(int new_count);
bool canApplyActivePlayerCount(int new_count) const;
bool isSessionDirty() const;
MultiplayerLayoutPlan buildLayoutPlan(int active_player_count);
```

Behavioral rules:
- `setActivePlayerCount(new_count)` shall reject values outside `2..4`.
- `setActivePlayerCount(new_count)` shall no-op when `new_count == active_player_count`.
- `setActivePlayerCount(new_count)` shall reset session gameplay state and clamp all navigation indices to the active range.
- `buildLayoutPlan(active_player_count)` shall be deterministic for the same input.

## 5. Acceptance Criteria

- **AC-001**: Given the firmware boots, When the multiplayer overview first appears, Then exactly 4 players shall be active and visible.
- **AC-002**: Given the user opens the multiplayer global menu, When the menu is shown, Then a `Players` option shall be available.
- **AC-003**: Given the user selects `Players`, When the selector screen opens, Then it shall offer `2 Players`, `3 Players`, `4 Players`, and `Back`.
- **AC-004**: Given a clean 4-player session, When the user selects `3 Players` and confirms if required, Then the overview shall immediately redraw with exactly 3 visible seat segments and only players `P1..P3` active.
- **AC-005**: Given a dirty session, When the user selects a different player count, Then the firmware shall request confirmation before discarding the current game.
- **AC-006**: Given the active player count is 2, When the overview is displayed, Then it shall render two opposing half-screen seat segments with readable, edge-oriented content.
- **AC-007**: Given the active player count is 3, When the overview is displayed, Then it shall render three evenly distributed third-screen seat segments with readable, edge-oriented content.
- **AC-008**: Given the active player count is 4, When the overview is displayed, Then it shall render four evenly distributed quarter-screen seat segments equivalent in behavior to the current four-player mode.
- **AC-009**: Given the active player count is 2 or 3, When commander-damage source selection opens, Then only active opponent seats shall be listed.
- **AC-010**: Given the active player count is 3, When global damage is applied, Then only the three active players shall lose life.
- **AC-011**: Given the active player count is 3, When the user resets the game, Then the session shall reset to new-game defaults for 3 players rather than reverting to 4.
- **AC-012**: Given the user power-cycles the device after previously using 2 or 3 players, When the device boots again, Then the active player count shall be 4.

## 6. Test Automation Strategy

- **Test Levels**: Unit, integration, device-level manual verification.
- **Frameworks**: Reuse the repository's existing C++ test approach where available; add lightweight host-side tests for layout planning and controller state transitions.
- **Test Data Management**: Construct fresh `MultiplayerGameState` instances per test case; avoid shared mutable state across tests.
- **CI/CD Integration**: Build validation shall include compilation for the multiplayer UI and controller code paths touched by APC support.
- **Coverage Requirements**: Unit coverage shall include APC validation, session reset semantics, active-player iteration bounds, and layout-plan generation for APC values `2`, `3`, and `4`.
- **Performance Testing**: Verify that overview refresh after APC changes completes within the same UI update cycle and does not introduce visible redraw flicker on device.

Recommended automated tests:
- Layout planner returns 2, 3, and 4 seat descriptors with correct angular sweep values.
- Controller rejects APC values below 2 and above 4.
- Changing APC resets life totals, commander tax, commander damage totals, names, and selection state.
- Selecting the current APC is a no-op.
- Commander-damage target lists and global damage loops honor `active_player_count`.

## 7. Rationale & Context

The current firmware is explicitly limited to four fixed quadrants. That works for four-player Commander but wastes space for smaller games and blocks support for tables with fewer players. The display is physically round, so a radial seat model matches the hardware better than a permanently rectangular quadrant model.

Using equal angular seat segments provides a direct mapping from player count to visual layout:
- 2 players naturally become halves.
- 3 players naturally become thirds.
- 4 players remain quarter-like and preserve the current mental model.

Keeping APC session-scoped avoids adding persistence complexity for a setup choice that is likely to change from game to game. Retaining a maximum backing capacity of four players minimizes code churn and allows the existing arrays and most controller code to remain structurally familiar.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: None required. This feature is fully local to the firmware runtime.

### Third-Party Services
- **SVC-001**: None.

### Infrastructure Dependencies
- **INF-001**: LVGL-based rendering primitives capable of creating one touch target and one independently rotated content container per seat segment.

### Data Dependencies
- **DAT-001**: In-memory gameplay state must add `active_player_count` and ensure loops use it consistently.

### Technology Platform Dependencies
- **PLT-001**: The implementation shall remain compatible with LVGL 8.4.x and the current ESP32-S3 firmware architecture.

### Compliance Dependencies
- **COM-001**: None.

## 9. Examples & Edge Cases

Example APC change flow:

```cpp
void apply_player_count_change(int new_count) {
    if (new_count < 2 || new_count > 4) return;
    if (new_count == state.active_player_count) return;

    if (controller.isSessionDirty()) {
        navigation.openPlayerCountConfirm(new_count);
        return;
    }

    controller.setActivePlayerCount(new_count);
    navigation.openMultiplayerOverview();
}
```

Example layout mapping:

```text
APC = 2 -> seat sweeps: [180, 180]
APC = 3 -> seat sweeps: [120, 120, 120]
APC = 4 -> seat sweeps: [90, 90, 90, 90]
```

Edge cases:
- If a stale UI event references player index `3` while APC is `3`, the event shall be ignored safely because valid indices are `0..2`.
- If APC changes while a life preview is pending, the preview shall be discarded as part of the session reset.
- If the user opens commander damage in a 2-player session, the source-selection screen shall present exactly one opponent choice.
- If the user selects `4 Players` while already in a 4-player session, no confirmation or reset shall occur.
- If APC is reduced from 4 to 2 and later increased back to 4 in the same runtime session, players `P3` and `P4` shall return with default new-game values rather than stale hidden state.

## 10. Validation Criteria

- The firmware builds successfully after introducing APC-aware state and UI changes.
- The overview screen renders correctly for APC values `2`, `3`, and `4` without overlapping primary content.
- All multiplayer actions operate only on active players.
- Manual device testing confirms that content orientation remains readable from each player's edge.
- Boot behavior confirms APC defaults to `4` after restart.
- Regression testing confirms four-player behavior remains functional after APC support is added.

## 11. Related Specifications / Further Reading

- [README.md](../README.md)
- [functional-spec.md](../docs/specifications/functional-spec.md)
- [spec-design-commander-tax.md](./spec-design-commander-tax.md)
- [spec-architecture-cpp-oop-refactor-plan.md](./spec-architecture-cpp-oop-refactor-plan.md)