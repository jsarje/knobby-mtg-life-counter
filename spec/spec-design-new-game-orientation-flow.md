---
title: New Game Flow and Multiplayer Orientation Layout (Design)
version: 1.0
date_created: 2026-04-05
last_updated: 2026-04-05
owner: Knobby MTG Life Counter Team
tags: [design, multiplayer, ui, navigation, layout]
---

# Introduction

This specification defines a staged New Game flow for multiplayer sessions and the orientation-aware overview layouts required to support that flow on the 360 x 360 round display. The flow replaces the current direct player-count change action with a two-step setup sequence: player-count selection first, then orientation selection for supported multiplayer counts.

## 1. Purpose & Scope

Purpose:
Define the UI flow, state contracts, controller behavior, and overview-layout rules required to support a New Game flow with player count selection and orientation selection.

Scope:
- Replace the multiplayer global-menu `Players` entry with `New Game`.
- Introduce a staged setup flow that chooses player count before applying a reset.
- Add orientation selection for 2-player, 3-player, and 4-player games.
- Define orientation-specific seat placement and content-orientation rules.
- Preserve existing destructive reset confirmation semantics when the current session is dirty.

Out of scope:
- Renaming players during the New Game flow.
- Persistent storage of player count or orientation.
- Changes to commander damage, commander tax, global damage, or brightness rules beyond compatibility updates.

Intended audience:
Firmware engineers, UI implementers, and test engineers modifying the multiplayer flow.

Assumptions:
- The product remains LVGL-based firmware on an ESP32-S3 target.
- The display remains a circular 360 x 360 panel with a bezel-constrained visible area.
- Maximum supported player capacity remains 4.

## 2. Definitions

- Active Player Count (APC): the number of active player seats in the current session. Valid values are 1, 2, 3, and 4.
- New Game Flow: the staged setup flow that chooses APC first and orientation second, then resets into a new session.
- Orientation Mode: one of the supported seat-orientation presets for a given APC.
- Same Direction: all seat content is oriented in the same reading direction.
- Opposite Sides: seat content is split between a top-facing group and a bottom-facing group.
- Round Table: seat content is oriented toward the display center, approximating players seated around the device.
- Dirty Session: multiplayer state that differs from new-game defaults for the current APC and orientation.
- Safe Radius: the inner circular radius available for readable content after bezel/safe-area inset is applied.

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: The multiplayer global menu shall replace the `Players` action with `New Game`.
- **REQ-002**: Selecting `New Game` shall open a first-page count selector with `1 Player`, `2 Players`, `3 Players`, `4 Players`, and `Back`.
- **REQ-003**: Selecting `1 Player` in the New Game flow shall skip the orientation page.
- **REQ-004**: Selecting `2 Players`, `3 Players`, or `4 Players` shall open a second-page orientation selector.
- **REQ-005**: The orientation selector shall offer exactly three choices for supported counts: `Same Direction`, `Opposite Sides`, and `Round Table`.
- **REQ-006**: If the user selects a count-orientation combination equal to the current session configuration, the firmware shall perform no reset and shall return to the multiplayer overview.
- **REQ-007**: If the current session is dirty and the user selects a different new-game configuration, the firmware shall require explicit confirmation before applying the reset.
- **REQ-008**: Applying a New Game configuration shall reset gameplay state, update active player count, update orientation mode, clear pending previews, and return to the multiplayer overview.
- **REQ-009**: The New Game confirmation screen shall describe the pending configuration as a new game action, not only as a player-count change.
- **REQ-010**: Multiplayer session state shall store the active orientation mode for the current game.
- **REQ-011**: Multiplayer session state shall also store pending New Game draft selections so the two-page flow can stage a count and orientation before apply.
- **REQ-012**: The multiplayer overview layout shall be computed from both APC and orientation mode.
- **REQ-013**: Layout generation shall explicitly define per-seat visibility, seat bounds, content bounds, and content rotation/alignment intent.
- **REQ-014**: The implementation shall keep touch targets coherent with visible seats after orientation changes.
- **REQ-015**: Seat content placement shall account for the circular visible area; text, life totals, and commander-tax badges shall remain readable and unclipped for all supported APC-orientation combinations.
- **REQ-016**: For `3 Players` with `Opposite Sides`, player 1 shall be the single top player, and players 2 and 3 shall be on the bottom side.
- **REQ-017**: For `4 Players` with `Opposite Sides`, players 1 and 2 shall be on the top side, and players 3 and 4 shall be on the bottom side.
- **REQ-018**: `Same Direction` shall orient all visible player content using one shared reading direction within the chosen layout.
- **REQ-019**: `Round Table` shall orient each visible seat toward the display center or as close as possible within LVGL readability limits.
- **REQ-020**: The implementation shall preserve the existing maximum player capacity of 4 and keep inactive player slots non-interactive and hidden.
- **REQ-021**: Existing rename, commander-damage, commander-tax, and global-damage features shall continue to operate only on active players after a New Game is applied.
- **CON-001**: The design shall remain usable on a 360 x 360 round display without requiring scrolling on the overview.
- **CON-002**: The design shall not require non-volatile persistence.
- **CON-003**: The implementation shall remain compatible with LVGL 8.4 and the current firmware architecture.
- **CON-004**: The overview may continue to use rectangular hit areas internally, but visible content placement shall respect circular safe-area limits.
- **GUD-001**: The New Game count page should visually indicate the currently active APC.
- **GUD-002**: The orientation page should visually indicate the currently active orientation when its APC matches the current session APC.
- **GUD-003**: Orientation-aware offsets should derive from safe-area metrics where practical, instead of relying only on hard-coded rectangular padding.
- **PAT-001**: The renderer should use a richer seat-layout contract than the current boolean top-facing heuristic.
- **PAT-002**: The controller should expose a single apply operation for New Game configuration so reset semantics live in one place.

## 4. Interfaces & Data Contracts

### 4.1 Orientation State Contract

The multiplayer domain shall define an explicit orientation enum and pending draft state.

```cpp
enum MultiplayerOrientationMode {
    MULTIPLAYER_ORIENTATION_SAME_DIRECTION = 0,
    MULTIPLAYER_ORIENTATION_OPPOSITE_SIDES = 1,
    MULTIPLAYER_ORIENTATION_ROUND_TABLE = 2
};

struct MultiplayerGameState {
    int active_player_count = 4;
    MultiplayerOrientationMode orientation_mode = MULTIPLAYER_ORIENTATION_ROUND_TABLE;
    int pending_player_count = -1;
    MultiplayerOrientationMode pending_orientation_mode = MULTIPLAYER_ORIENTATION_ROUND_TABLE;
    bool pending_orientation_valid = false;
};
```

State rules:
- `orientation_mode` shall always contain a valid enum value.
- `pending_player_count == -1` means no draft APC is staged.
- `pending_orientation_valid == false` means no draft orientation is staged.
- When `active_player_count == 1`, the active orientation shall be treated as implementation-defined but stable; the orientation page is not shown.

### 4.2 Navigation Contract

The New Game flow shall follow this staged navigation model:

```text
Global Menu -> New Game Count -> Orientation (2p/3p/4p only) -> Confirm if dirty -> Multiplayer Overview
```

Required navigation operations:

```cpp
void openNewGameCountScreen();
void openNewGameOrientationScreen(int pending_count);
void openNewGameConfirmScreen();
```

Navigation rules:
- Back from the count page shall return to the multiplayer global menu.
- Back from the orientation page shall return to the count page.
- Back from the confirmation page shall return to the most recent New Game setup page.
- Confirm shall apply the pending count and orientation together.

### 4.3 Orientation Selector Contract

The orientation page shall present the following options for APC values 2, 3, and 4:

```text
Same Direction
Opposite Sides
Round Table
Back
```

Selection rules:
- Choosing an orientation shall either apply immediately for a clean session or advance to confirmation for a dirty session.
- The pending APC chosen on page 1 shall remain unchanged while the user is on page 2.

### 4.4 Seat Layout Contract

The overview renderer shall consume an orientation-aware seat description.

```cpp
enum SeatContentAnchor {
    SEAT_ANCHOR_TOP = 0,
    SEAT_ANCHOR_BOTTOM = 1,
    SEAT_ANCHOR_LEFT = 2,
    SEAT_ANCHOR_RIGHT = 3,
    SEAT_ANCHOR_CENTER = 4
};

struct SeatMetrics {
    bool visible;
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    int16_t content_rotation_deg;
    SeatContentAnchor primary_anchor;
    lv_coord_t content_pad_x;
    lv_coord_t content_pad_y;
    lv_coord_t content_offset_x;
    lv_coord_t content_offset_y;
};
```

Layout rules:
- Seat order shall remain player-index order: player 1 is index 0, player 2 is index 1, player 3 is index 2, player 4 is index 3.
- Layout generation shall map player indices deterministically for every APC-orientation combination.
- `3p Opposite Sides` mapping shall be `P1 top`, `P2 bottom-left`, `P3 bottom-right`.
- `4p Opposite Sides` mapping shall be `P1 top-left`, `P2 top-right`, `P3 bottom-right`, `P4 bottom-left`.
- `2p Opposite Sides` mapping shall place `P1` on top and `P2` on bottom.
- `Same Direction` may reuse the same seat rectangles as the APC base layout but shall use one shared content reading direction.
- `Round Table` may keep rectangular seat bounds, but each seat's content alignment and rotation intent shall approximate radial inward-facing readability.

### 4.5 Controller/API Contract

Required controller operations:

```cpp
bool canApplyNewGameConfig(int player_count, MultiplayerOrientationMode orientation) const;
bool isCurrentNewGameConfig(int player_count, MultiplayerOrientationMode orientation) const;
void stageNewGamePlayerCount(int player_count);
void stageNewGameOrientation(MultiplayerOrientationMode orientation);
void clearPendingNewGameConfig();
void applyNewGameConfig(int player_count, MultiplayerOrientationMode orientation);
```

Behavior rules:
- Validation shall reject unsupported player counts.
- Validation shall reject orientation-dependent apply attempts when APC is outside `2..4`.
- `applyNewGameConfig` shall reset gameplay state using the new APC and then persist the selected orientation as the active orientation for that session.
- `applyNewGameConfig` shall no-op when the requested configuration already matches the current session.

## 5. Acceptance Criteria

- **AC-001**: Given the user opens the multiplayer global menu, When the menu renders, Then `New Game` shall be shown and `Players` shall not be shown.
- **AC-002**: Given the user selects `New Game`, When the first page opens, Then the page shall offer `1 Player`, `2 Players`, `3 Players`, `4 Players`, and `Back`.
- **AC-003**: Given the user selects `1 Player`, When the current session is clean, Then the firmware shall apply a new 1-player game without showing the orientation page.
- **AC-004**: Given the user selects `2 Players`, `3 Players`, or `4 Players`, When the second page opens, Then it shall offer `Same Direction`, `Opposite Sides`, `Round Table`, and `Back`.
- **AC-005**: Given a dirty current session, When the user finishes a New Game selection that differs from the current configuration, Then the firmware shall show confirmation before resetting.
- **AC-006**: Given a clean current session, When the user finishes a New Game selection that differs from the current configuration, Then the firmware shall apply it immediately.
- **AC-007**: Given the selected New Game configuration matches the current APC and orientation, When the selection is completed, Then the firmware shall leave the session unchanged and return to the multiplayer overview.
- **AC-008**: Given `3 Players` and `Opposite Sides`, When the multiplayer overview renders, Then player 1 shall occupy the top seat and players 2 and 3 shall occupy the two bottom seats.
- **AC-009**: Given `4 Players` and `Opposite Sides`, When the multiplayer overview renders, Then players 1 and 2 shall be on the top side and players 3 and 4 shall be on the bottom side.
- **AC-010**: Given any supported APC with `Same Direction`, When the overview renders, Then all visible seats shall use the same content reading direction.
- **AC-011**: Given any supported APC with `Round Table`, When the overview renders, Then each visible seat shall be oriented toward the center or the closest readable approximation supported by the renderer.
- **AC-012**: Given any APC-orientation combination, When the overview renders on hardware, Then life totals, names, and commander-tax badges shall remain fully visible within the circular display.
- **AC-013**: Given a New Game is applied, When the user enters rename, commander damage, commander tax, or global damage flows, Then only active players shall participate.

## 6. Test Automation Strategy

- **Test Levels**: Unit, integration, device-level manual verification.
- **Frameworks**: Reuse the repository's existing firmware build validation and add host-side controller/layout tests where feasible.
- **Test Data Management**: Create fresh `MultiplayerGameState` instances for each layout and controller test case.
- **CI/CD Integration**: Validate compilation of all touched multiplayer UI, controller, and navigation code paths.
- **Coverage Requirements**: Cover APC-orientation validation, staged pending-state transitions, no-op selection behavior, and deterministic layout generation for all supported APC-orientation pairs.
- **Performance Testing**: Verify that switching to a new game and redrawing the overview does not introduce visible flicker or stale seat content.

Recommended automated tests:
- Controller accepts valid APC-orientation combinations and rejects invalid ones.
- Selecting the current configuration is a no-op.
- Dirty-session flow stages pending APC and pending orientation without applying them until confirm.
- Clean-session flow applies the pending configuration immediately.
- Layout generation returns expected seat visibility and anchor/rotation metadata for 2p, 3p, and 4p across all three orientations.

## 7. Rationale & Context

The current player-count flow is destructive and single-step. That model is too narrow once seat orientation becomes part of game setup. A staged New Game flow makes the reset semantics explicit and gives the player count and orientation equal status as part of one setup decision.

The current multiplayer renderer is built around rectangular regions and a simple top-facing versus bottom-facing heuristic. That is insufficient for `Same Direction` and `Round Table` presets. The new contract therefore requires explicit seat metadata so the renderer can position content with more control while still respecting the round display's safe area.

The opposite-sides mappings are intentionally deterministic and player-order preserving:
- `3p Opposite Sides`: player 1 is the solo top seat.
- `4p Opposite Sides`: players 1 and 2 are the top row.

These rules keep player order predictable and avoid forcing the implementation to infer seat ownership from geometric heuristics.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: None. This feature is fully local to the firmware runtime.

### Third-Party Services
- **SVC-001**: None.

### Infrastructure Dependencies
- **INF-001**: LVGL screen, button, label, and transform/alignment primitives are required to render the staged New Game flow and multiplayer overview.

### Data Dependencies
- **DAT-001**: The multiplayer state model must add active and pending orientation fields alongside pending player count.

### Technology Platform Dependencies
- **PLT-001**: The implementation shall remain compatible with the current ESP32-S3 firmware architecture and LVGL 8.4.x.

### Compliance Dependencies
- **COM-001**: None.

## 9. Examples & Edge Cases

Example staged flow:

```cpp
void finish_new_game_selection(int player_count, MultiplayerOrientationMode orientation) {
    if (!controller.canApplyNewGameConfig(player_count, orientation)) return;

    if (controller.isCurrentNewGameConfig(player_count, orientation)) {
        navigation.openMultiplayerScreen();
        return;
    }

    if (controller.isSessionDirty()) {
        controller.stageNewGamePlayerCount(player_count);
        controller.stageNewGameOrientation(orientation);
        navigation.openNewGameConfirmScreen();
        return;
    }

    controller.applyNewGameConfig(player_count, orientation);
    navigation.openMultiplayerScreen();
}
```

Edge cases:
- Selecting `1 Player` shall not require the orientation page.
- Backing out of the orientation page shall preserve the staged count until the user changes it or exits the flow.
- Returning from confirmation shall not clear the staged count or orientation.
- Round-table offsets may need APC-specific tuning so content remains inside the circular safe radius.
- Commander-tax badges shall remain visible even when the seat's content is rotated or inward-facing.

## 10. Validation Criteria

- The firmware compiles successfully after introducing New Game state, navigation, and layout changes.
- The global menu, New Game pages, and confirm screen follow the documented flow.
- All APC-orientation combinations render without hidden or clipped critical content on the round display.
- `3p Opposite Sides` and `4p Opposite Sides` obey the exact player ordering defined in this specification.
- Existing multiplayer interactions continue to work after a New Game is applied.

## 11. Related Specifications / Further Reading

- [spec/spec-design-variable-player-count.md](spec-design-variable-player-count.md)
- [docs/specifications/functional-spec.md](../docs/specifications/functional-spec.md)