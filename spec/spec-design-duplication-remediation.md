---
title: Duplication Remediation and Shared UI Construction Patterns
version: 1.0
date_created: 2026-04-04
last_updated: 2026-04-04
owner: Knobby MTG Life Counter Team
tags: [design, refactor, duplication, lvgl, ui, navigation, firmware]
---

# Introduction

This specification defines the requirements, constraints, and interfaces for reducing high-value code duplication in the Knobby firmware. The focus is behavior-preserving consolidation of repeated LVGL screen construction, repeated secondary-screen widget composition, repeated navigation orchestration, and repeated glue logic around refresh and validation. The goal is to improve maintainability, consistency, and reviewability without changing gameplay behavior, hardware assumptions, or the current product scope.

## 1. Purpose & Scope

Purpose:

- Define a formal, implementation-ready design for removing the most harmful duplication and code smells identified in the current firmware review.
- Establish the remediation boundaries, reusable patterns, and acceptance criteria for future implementation tasks.
- Prioritize repeated UI and navigation patterns over broad architectural redesign.

Scope:

- Shared construction of LVGL root screens.
- Shared construction of recurring LVGL widgets such as title labels, action buttons, and value-plus-hint layouts.
- Consolidation of duplicated secondary-screen composition patterns.
- Consolidation of repeated navigation transition flow.
- Consolidation of repeated refresh wrapper and validation helper patterns where they directly support duplication reduction.

Out of scope:

- New gameplay features.
- Hardware redesign or board-support changes.
- LVGL major-version migration.
- Full replacement of global singleton usage.
- Full object-model redesign beyond what is necessary to remove the targeted duplication.
- Rewriting the multiplayer overview seat-layout algorithm into a generic framework without evidence of repeated need.

Intended audience:

- Firmware maintainers implementing refactors.
- Reviewers validating refactor safety.
- AI coding agents performing bounded implementation tasks.

Assumptions:

- The firmware remains Arduino-based on ESP32-S3 hardware.
- LVGL 8.4.x remains the required UI framework.
- Existing multiplayer, settings, and intro-screen behaviors are the baseline to preserve.

## 2. Definitions

- **Behavior-Preserving Refactor**: A source-code restructuring that does not intentionally change externally visible runtime behavior.
- **Duplication Hotspot**: A repeated code pattern with enough volume, risk, or maintenance cost to justify targeted consolidation.
- **Base Screen Helper**: A reusable function or component that constructs the common LVGL root screen object and applies standard base styling.
- **Secondary Screen**: Any non-overview screen used for menus, rename flow, damage adjustment, player-count selection, or settings.
- **UI Primitive Builder**: A reusable helper that constructs a common widget type or widget cluster with fixed styling and standard behavior.
- **Navigation Transition Flow**: The repeated sequence of preview commit, state setup, screen refresh, input flush, and screen load performed before a screen change.
- **Refresh Wrapper**: A thin helper function whose only purpose is forwarding current state into a screen `refresh()` call.
- **Validation Helper**: A reusable method that centralizes repeated precondition checks such as active-player index validation.
- **LVGL**: Light and Versatile Graphics Library used for embedded UI rendering and events.
- **Commander Damage Flow**: The menu, source selection, and damage adjustment sequence used to track commander damage between players.

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: The remediation shall preserve all current user-visible behavior for intro, multiplayer, menu, rename, commander-damage, all-damage, player-count, and settings flows.
- **REQ-002**: The remediation shall eliminate repeated LVGL root-screen construction by introducing one shared base-screen construction mechanism.
- **REQ-003**: The remediation shall eliminate repeated title-label construction by introducing one shared title-label builder or equivalent reusable abstraction.
- **REQ-004**: The remediation shall eliminate repeated action-button construction by introducing one shared button builder or equivalent reusable abstraction.
- **REQ-005**: The remediation shall eliminate repeated value-plus-hint screen composition for the commander-damage and all-damage screens by introducing one shared composition helper or reusable pattern.
- **REQ-006**: The remediation shall centralize repeated navigation transition flow inside the navigation layer so public `openXxx` methods do not repeat the same orchestration template.
- **REQ-007**: The remediation shall remove the redundant multiplayer refresh currently performed during multiplayer-screen navigation when the same refresh has already occurred in the shared pre-navigation path.
- **REQ-008**: The remediation shall keep LVGL event types, callback wiring semantics, and widget placement behavior unchanged unless a later specification explicitly changes them.
- **REQ-009**: The remediation shall preserve the current specialized multiplayer overview layout logic and shall not force it into a generic screen-builder pattern if doing so would reduce clarity.
- **REQ-010**: The remediation shall reduce repeated thin refresh-wrapper functions where a shared grouped refresh mechanism can provide the same behavior with clearer ownership.
- **REQ-011**: The remediation shall centralize repeated active-player validation checks where the same preconditions are enforced in multiple controller paths.
- **REQ-012**: The remediation shall preserve deterministic embedded-friendly ownership and shall not introduce fragile runtime allocation patterns solely for abstraction purposes.
- **REQ-013**: The remediation shall be executable incrementally so that each phase can be reviewed and validated independently.
- **REQ-014**: The remediation shall preserve compatibility with current LVGL 8.4.x APIs and the current ESP32 firmware environment.
- **REQ-015**: The remediation shall document which repeated patterns are intentionally not consolidated and why.

- **CON-001**: The implementation shall remain compatible with ESP32-S3 and the current display, touch, and knob-input model.
- **CON-002**: The implementation shall remain compatible with LVGL 8.4.x.
- **CON-003**: The remediation shall not change screen geometry, text hierarchy, control placement, or color intent unless such change is required to preserve a shared helper contract already matching the current behavior.
- **CON-004**: The remediation shall prefer free functions or small helper structs over inheritance-heavy redesign for the initial pass.
- **CON-005**: The remediation shall not introduce broad dependency injection or singleton-removal work beyond what is necessary for the duplicated paths.
- **CON-006**: The remediation shall keep the multiplayer overview seat-layout planner and quadrant composition logic separate from generic secondary-screen helpers.
- **CON-007**: Any callback user-data refactor shall preserve the current mapping from UI interaction to player or row selection.

- **GUD-001**: Consolidate the highest-volume duplication first, even if smaller smells remain temporarily.
- **GUD-002**: Prefer naming helpers by UI role, such as base screen, title label, action button, or value-hint pair, rather than by file of origin.
- **GUD-003**: Keep helper APIs narrow and parameterized only by values that genuinely vary between current call sites.
- **GUD-004**: Prefer behavior-preserving extraction over speculative generalization.
- **GUD-005**: Keep the main multiplayer overview logic readable and domain-specific; do not abstract it merely for symmetry with simpler screens.
- **GUD-006**: Validation helpers should live close to the state or controller logic they protect rather than in UI code.
- **GUD-007**: Navigation should own transition mechanics; controllers should own game rules; screens should own LVGL composition and refresh.

- **PAT-001**: Use a shared base-screen helper for all screens that currently create a 360 x 360 black root LVGL object with no border and no scrollbar.
- **PAT-002**: Use a shared widget-builder layer for repeated title labels, standard buttons, and value-hint label pairs.
- **PAT-003**: Use small composite builders for repeated secondary-screen layouts rather than one monolithic UI factory.
- **PAT-004**: Use one internal navigation template that accepts screen-specific state setup and refresh work.
- **PAT-005**: Use grouped refresh helpers or a lightweight refresh registry where multiple screens need synchronized refresh from one state change.

## 4. Interfaces & Data Contracts

### 4.1 Base Screen Construction Contract

The implementation shall provide one reusable contract for creating standard root screens.

```cpp
lv_obj_t* create_base_screen();
```

Behavioral contract:

- Creates an LVGL root object.
- Sets size to `360 x 360`.
- Sets background color to black.
- Sets border width to `0`.
- Sets scrollbar mode to `LV_SCROLLBAR_MODE_OFF`.
- Returns the configured screen object.
- Does not attach screen-specific child widgets.
- Does not load the screen.

### 4.2 Shared Widget Builder Contracts

The implementation shall provide reusable contracts for repeated widget construction patterns.

```cpp
lv_obj_t* create_title_label(lv_obj_t* parent, const char* text, lv_coord_t y_offset);

lv_obj_t* create_action_button(
    lv_obj_t* parent,
    const char* text,
    lv_coord_t width,
    lv_coord_t height,
    lv_event_cb_t callback,
    lv_event_code_t event_code = LV_EVENT_CLICKED
);
```

Required behavior:

- Title labels use the current title styling pattern used by secondary screens.
- Action buttons preserve current text centering and callback registration semantics.
- Optional style variations such as circular back buttons may be expressed through a small extension contract rather than reintroducing manual duplication.

Optional extension contract:

```cpp
struct ButtonStyleOptions {
    bool circular_back_style = false;
    lv_coord_t radius = LV_RADIUS_CIRCLE;
    lv_coord_t pad_all = 0;
};

lv_obj_t* create_action_button_styled(
    lv_obj_t* parent,
    const char* text,
    lv_coord_t width,
    lv_coord_t height,
    lv_event_cb_t callback,
    lv_event_code_t event_code,
    const ButtonStyleOptions& options
);
```

### 4.3 Value-and-Hint Layout Contract

The implementation shall provide one reusable builder for screens that display a large value and a secondary hint.

```cpp
struct ValueHintLayout {
    lv_obj_t* title_label;
    lv_obj_t* value_label;
    lv_obj_t* hint_label;
};

ValueHintLayout create_value_hint_screen(
    lv_obj_t* parent,
    const char* title_text,
    const char* hint_text,
    lv_coord_t title_y,
    lv_coord_t value_y,
    lv_coord_t hint_y
);
```

Required behavior:

- The builder creates the title, value, and hint labels with the current styling semantics used by commander-damage and all-damage screens.
- The caller remains responsible for setting dynamic value text during `refresh()`.
- The builder shall not own button creation for apply or back actions, because those differ by screen.

### 4.4 Navigation Transition Contract

The navigation layer shall expose a single internal transition template.

```cpp
void navigate_to(
    lv_obj_t* screen,
    void (*state_setup_fn)(),
    void (*refresh_fn)()
);
```

Equivalent callable-based forms are permitted if they remain embedded-safe and reviewable.

Required behavior:

- Commits pending multiplayer life preview before leaving the current flow when applicable.
- Refreshes the multiplayer overview as needed after preview commit.
- Executes screen-specific state setup.
- Executes screen-specific refresh.
- Flushes stale input via the existing flush callback.
- Loads the target screen unless it is already active.

Forbidden behavior:

- Duplicating the same pre-navigation template in each public `openXxx` method after this helper exists.
- Embedding screen-specific business rules directly in the low-level screen-load routine.

### 4.5 Refresh Coordination Contract

The implementation shall replace repeated one-line refresh wrappers with one grouped mechanism where practical.

Allowed forms:

```cpp
void refresh_multiplayer_related_screens();
void refresh_settings_screen();
void refresh_intro_screen();
```

or

```cpp
enum class ScreenRefreshGroup {
    Intro,
    Settings,
    MultiplayerCore,
    MultiplayerMenus
};

void refresh_group(ScreenRefreshGroup group);
```

Required behavior:

- The resulting mechanism shall remain explicit about which screens are refreshed together.
- State source selection shall remain deterministic and not depend on hidden global side effects introduced by the refactor.

### 4.6 Validation Helper Contract

Repeated active-player validation shall be centralized behind small reusable methods.

Possible contract examples:

```cpp
bool is_valid_cmd_damage_pair(int source, int target) const;
bool is_valid_menu_player() const;
```

Required behavior:

- The helper names shall reflect the protected operation.
- The helpers shall preserve current active-player-range semantics.
- The helpers shall not move domain validation into the screen layer.

### 4.7 Targeted Component Coverage

The following current components are in scope for the duplication-remediation design:

| Component | In Scope | Primary Concern |
| --- | --- | --- |
| Intro screen | Yes | repeated base-screen construction |
| Settings screen | Yes | repeated base-screen, title, button composition |
| Multiplayer overview | Partial | repeated base-screen only; preserve specialized overview layout |
| Multiplayer menu screen | Yes | repeated base-screen, title, button stack |
| Rename screen | Yes | repeated base-screen, title, action buttons |
| Commander target select screen | Yes | repeated base-screen, title, repeated button-row composition |
| Commander damage screen | Yes | repeated base-screen, title, value-hint layout |
| All damage screen | Yes | repeated base-screen, title, value-hint layout |
| Player-count screen | Yes | repeated base-screen, title, button stack |
| Player-count confirm screen | Yes | repeated base-screen, title, action buttons |
| Navigation controller | Yes | repeated transition orchestration |
| Controller validation helpers | Yes | repeated active-player precondition checks |
| Multiplayer seat layout algorithm | Limited | preserve current specialized logic rather than genericizing it |

## 5. Acceptance Criteria

- **AC-001**: Given any screen that currently uses the standard black 360 x 360 root object, When the refactor is complete, Then that screen shall obtain its root object through one shared base-screen construction mechanism.
- **AC-002**: Given the secondary screens that currently create title labels manually, When the refactor is complete, Then title-label creation shall be performed through one shared reusable builder.
- **AC-003**: Given the screens that currently create standard action buttons manually or through file-local helpers, When the refactor is complete, Then standard button creation shall be performed through one shared reusable builder.
- **AC-004**: Given the commander-damage and all-damage screens, When the refactor is complete, Then the repeated value-plus-hint composition shall be expressed through one shared layout builder or one shared construction pattern.
- **AC-005**: Given the navigation controller, When any public `openXxx` method is inspected after refactor, Then the repeated preview-commit, refresh, flush, and load template shall be centralized rather than duplicated per method.
- **AC-006**: Given navigation to the multiplayer overview, When the open method runs, Then the multiplayer screen shall not be redundantly refreshed twice in the same transition path.
- **AC-007**: Given the multiplayer overview screen, When the refactor is complete, Then its seat-layout and orientation logic shall remain behaviorally equivalent to the current implementation.
- **AC-008**: Given a user interaction that selects a player, opens a player menu, adjusts life, applies commander damage, applies all-player damage, changes player count, or opens settings, When the refactor is complete, Then the resulting behavior shall match the current baseline flow.
- **AC-009**: Given repeated refresh wrappers in the input-dispatch layer, When the refactor is complete, Then the repeated one-line wrapper pattern shall be reduced to a clearer grouped mechanism without loss of refresh coverage.
- **AC-010**: Given repeated active-player validation in controller methods, When the refactor is complete, Then those checks shall be expressed through reusable operation-specific validation helpers or equivalent centralized checks.
- **AC-011**: Given review of the resulting code, When a maintainer compares repeated UI construction across settings and multiplayer secondary screens, Then the number of repeated construction sites shall be materially lower and the shared helpers shall be identifiable by name.
- **AC-012**: Given this specification, When an implementation task is created, Then the task can target a bounded subset of the work without requiring unstated assumptions from the original code review.

## 6. Test Automation Strategy

- **Test Levels**: Static review, targeted unit-style host tests where practical for controller helpers, integration-level firmware build verification, and on-device manual flow validation.
- **Frameworks**: Reuse current repository review workflows and any existing C++ test approach available to the maintainers; add lightweight host-side tests only for logic that becomes sufficiently isolated.
- **Test Data Management**: Use deterministic `SettingsState` and `MultiplayerGameState` fixtures for any host-side tests of validation or navigation-preparation logic.
- **CI/CD Integration**: The remediation should compile through the normal firmware build path and should remain small enough for focused code review per phase.
- **Coverage Requirements**: No explicit numeric coverage threshold is required by this specification; extracted logic-heavy helpers should be tested first if unit tests are added.
- **Performance Testing**: Validate that screen creation, refresh cadence, and input responsiveness remain equivalent on device after helper extraction.

Recommended verification focus:

- Base-screen helper produces equivalent root-screen configuration.
- Shared title and button helpers preserve current styling and event behavior.
- Commander-damage and all-damage screens still render the correct dynamic values and buttons.
- Navigation still commits pending life preview correctly before transitions.
- No stale encoder input is applied to the next screen after a transition.
- Player-count, rename, commander-tax, commander-damage, and settings-brightness flows remain unchanged.

## 7. Rationale & Context

The current firmware already has a workable behavioral baseline, but several areas exhibit clear high-volume duplication. The most severe duplication is in LVGL screen setup and repeated secondary-screen widget composition. That duplication raises maintenance cost because styling or layout adjustments require editing many call sites and reviewing for drift.

The repeated navigation template is a second-order problem because it duplicates logic while also mixing responsibilities. Screen transitions, preview commitment, state mutation, and screen refresh are intertwined in a way that makes simple changes broader than necessary. Consolidating that orchestration improves both DRY compliance and responsibility clarity.

This specification intentionally avoids turning the remediation into a full architectural rewrite. The codebase already has related architecture work documented elsewhere, but the highest-value next step for this branch is a narrow, behavior-preserving reduction of repeated code. Free-function helpers and small reusable builders are the correct first tool because they remove duplication with low migration risk and minimal embedded overhead.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: ESP32-S3 runtime environment - required to preserve current firmware execution and hardware interaction behavior.

### Third-Party Services
- **SVC-001**: LVGL - required for all screen, widget, event, and timer behavior covered by this specification.

### Infrastructure Dependencies
- **INF-001**: Arduino firmware lifecycle - required because navigation and screen construction continue to run within the existing firmware app model.
- **INF-002**: Existing repository build and review workflow - required to validate incremental refactor phases.

### Data Dependencies
- **DAT-001**: In-memory settings and multiplayer session state - required to preserve refresh behavior and controller validation semantics.

### Technology Platform Dependencies
- **PLT-001**: C++ compilation in the current firmware environment - required for helper extraction and controller-layer cleanup.
- **PLT-002**: LVGL 8.4.x API compatibility - required because existing screens and rendering contracts depend on this API surface.

### Compliance Dependencies
- **COM-001**: None identified beyond repository contribution and review expectations.

## 9. Examples & Edge Cases

```cpp
// Example: shared base-screen helper usage
void SettingsScreen::create(lv_event_cb_t back_cb) {
    screen_ = create_base_screen();
    label_title_ = create_title_label(screen_, "knobby", 24);
    // additional settings-specific widgets...
}

// Example: centralized navigation template usage
void NavigationController::openSettingsScreen() {
    navigate_to(
        g_screen_settings.lvObject(),
        []() {
            g_settings_controller.updateBattery(true);
            g_settings_state.battery_percent = g_settings_controller.readBatteryPercent();
        },
        []() {
            g_screen_settings.refresh(g_settings_state);
        }
    );
}
```

Edge cases to preserve:

- The multiplayer overview still supports touch selection and long-press menu entry with the current player-index semantics.
- Circular or specially styled back buttons remain visually distinct where currently used.
- Commander-damage and all-damage screens may share layout construction without forcing identical button arrangements.
- A screen with one-off widget geometry may continue using direct LVGL calls for those unique parts while still using shared primitives for the repeated parts.
- Validation helpers must not silently broaden or narrow the set of legal player indices.
- The grouped refresh mechanism must not omit screens that currently rely on synchronized refresh after state changes.

## 10. Validation Criteria

- The specification is self-contained and understandable without access to the originating chat.
- The specification identifies the major duplication hotspots and binds each one to explicit requirements.
- The specification distinguishes in-scope consolidation from intentionally preserved specialized logic.
- The specification defines reusable interface contracts for screen construction, widget construction, navigation flow, and validation cleanup.
- The specification provides acceptance criteria that can guide code review and manual verification.
- The specification remains consistent with current firmware architecture constraints and documented LVGL requirements.

## 11. Related Specifications / Further Reading

- `spec/spec-architecture-cpp-oop-refactor-plan.md`
- `spec/spec-process-cpp-oop-refactor-phases.md`
- `spec/spec-design-variable-player-count.md`
- `docs/specifications/system-spec.md`
- `docs/specifications/functional-spec.md`