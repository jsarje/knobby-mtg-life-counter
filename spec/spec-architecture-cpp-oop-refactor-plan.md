---
title: C++ Object-Oriented Firmware Refactor Architecture Plan
version: 1.0
date_created: 2026-03-26
last_updated: 2026-03-26
owner: jsarje
tags: [architecture, design, firmware, cpp, oop, lvgl, esp32]
---

# Introduction

This specification defines the target high-level architecture for refactoring the firmware from a primarily C-style, file-scope-static design into a C++ object-oriented design. The goal is to improve separation of concerns, testability, ownership clarity, and maintainability while preserving the current multiplayer commander life-counter behavior and existing hardware assumptions.

## 1. Purpose & Scope

This specification describes the intended architectural end state for the firmware after a full object-oriented refactor.

Scope:

- The runtime architecture of the application
- Module and class responsibilities
- Object ownership and lifecycle rules
- Boundaries between hardware abstraction, application state, and UI composition
- Incremental compatibility constraints needed during migration

Out of scope:

- Detailed code implementation for each phase
- New gameplay features unrelated to the architectural migration
- Hardware redesign
- LVGL major-version upgrade beyond the currently required 8.4.x line

Intended audience:

- Maintainers implementing the refactor
- AI coding agents generating phased changes
- Reviewers validating the architecture against firmware constraints

Assumptions:

- The firmware remains Arduino-based on ESP32-S3 hardware
- LVGL 8.4.x remains the UI framework during the refactor
- Existing product behavior remains the baseline unless a later specification explicitly changes it

## 2. Definitions

- **ADC**: Analog-to-Digital Converter.
- **App Bootstrap**: Startup and loop orchestration currently delegated from `knobby.ino` into `knobby_app.cpp`.
- **C ABI**: C Application Binary Interface. Used where C and C++ modules must interoperate.
- **Controller**: A class responsible for coordinating user actions, state changes, and view refreshes.
- **HAL**: Hardware Abstraction Layer.
- **LVGL**: Light and Versatile Graphics Library used for the embedded UI.
- **Model**: A class or data structure that owns application state independent of rendering.
- **Screen**: A UI surface or view corresponding to a product flow such as multiplayer overview or settings.
- **Service**: A non-UI component that encapsulates hardware access or business logic.
- **State Store**: The aggregate owner of persistent in-memory runtime state for a game session.
- **View**: A class responsible for creating and updating LVGL objects without owning business rules.

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: The refactor shall preserve the current multiplayer commander firmware behavior as the functional baseline.
- **REQ-002**: The architecture shall separate hardware access, application state, and UI rendering into distinct component layers.
- **REQ-003**: The architecture shall replace file-scope mutable globals with explicit object ownership wherever practical.
- **REQ-004**: The architecture shall support incremental migration so that legacy C modules and new C++ classes can coexist temporarily.
- **REQ-005**: The architecture shall keep `knobby.ino` as a thin entrypoint that delegates to a dedicated application object.
- **REQ-006**: The architecture shall introduce a central application root object responsible for constructing, wiring, starting, and advancing runtime components.
- **REQ-007**: The architecture shall represent gameplay state through typed domain objects rather than distributed primitive globals.
- **REQ-008**: The architecture shall isolate LVGL widget construction and refresh behavior from business state mutation logic.
- **REQ-009**: The architecture shall centralize board-facing functions such as display startup, touch input, encoder input, and battery sampling behind service interfaces or concrete service classes.
- **REQ-010**: The architecture shall preserve C linkage only where required for interoperability with remaining C modules during migration.

- **CON-001**: The firmware shall remain compatible with ESP32-S3 and the current display, touch, and encoder hardware assumptions.
- **CON-002**: The firmware shall remain compatible with LVGL 8.4.x during this refactor.
- **CON-003**: The refactor shall not require a functional PlatformIO build for acceptance in this branch; documentation and code planning must be self-consistent without build validation.
- **CON-004**: The migration shall avoid introducing dynamic ownership patterns that are fragile on embedded hardware.
- **CON-005**: The migration shall prefer deterministic lifecycle management and static-or-root-owned objects over ad hoc heap ownership.

- **GUD-001**: Prefer composition over inheritance for firmware modules.
- **GUD-002**: Keep class interfaces narrow and oriented around responsibilities rather than implementation details.
- **GUD-003**: Prefer one-way dependencies: hardware services -> domain/application services -> UI controllers/views.
- **GUD-004**: Prefer explicit `init`, `start`, `tick`, and `refresh` style methods for embedded runtime orchestration.
- **GUD-005**: Avoid embedding LVGL object mutation inside low-level input callbacks; queue or dispatch into the app loop instead.

- **PAT-001**: Introduce an `Application` root that owns subsystem instances such as platform services, state store, controllers, and screens.
- **PAT-002**: Use screen classes for LVGL object composition and view updates.
- **PAT-003**: Use domain/service classes for battery logic, brightness logic, multiplayer game state, and commander damage rules.
- **PAT-004**: Use adapters around legacy C APIs during transition rather than performing a full rewrite in one step.

## 4. Interfaces & Data Contracts

### 4.1 Target Architectural Layers

| Layer | Responsibility | Typical Components |
| --- | --- | --- |
| Entry | Minimal firmware entrypoint | `knobby.ino` |
| Application | Startup, dependency wiring, loop orchestration | `Application`, `AppContext` |
| Platform Services | Hardware-specific access and LVGL/display integration | `DisplayService`, `InputService`, `BatteryService`, `BacklightService` |
| Domain | Multiplayer state and gameplay rules | `GameState`, `PlayerState`, `CommanderDamageMatrix`, `SettingsState` |
| Presentation | Screens, controllers, and view models | `MultiplayerScreen`, `SettingsScreen`, `IntroScreen`, `NavigationController` |
| Compatibility | C/C++ bridges used only during migration | legacy adapters and exported C ABI functions |

### 4.2 Target Application Root Contract

```cpp
class Application {
public:
  bool init();
  void start();
  void tick();
};
```

Behavioral contract:

- `init()` performs one-time dependency construction and hardware/service initialization.
- `start()` creates the initial UI flow after successful initialization.
- `tick()` performs queued input processing, timer/service advancement, and LVGL pumping.

### 4.3 Target State Ownership Model

```text
Application
├── PlatformServices
│   ├── DisplayService
│   ├── TouchInputService
│   ├── EncoderInputService
│   ├── BatteryService
│   └── BacklightService
├── SessionState
│   ├── SettingsState
│   └── MultiplayerGameState
└── UiCoordinator
    ├── IntroScreen
    ├── MultiplayerScreen
    ├── MenuScreen
    ├── RenameScreen
    ├── CommanderDamageScreens
    └── SettingsScreen
```

### 4.4 Domain Data Contract Expectations

| Domain Type | Required Fields | Notes |
| --- | --- | --- |
| `PlayerState` | `name`, `life_total`, `selected` | UI-neutral player information |
| `MultiplayerGameState` | `players[4]`, `selected_player`, `pending_life_delta`, `preview_player` | Owns commander multiplayer flow state |
| `CommanderDamageMatrix` | `damage[4][4]` | Tracks source-to-target commander damage totals |
| `SettingsState` | `brightness_percent`, `battery_voltage`, `battery_percent`, `battery_valid` | Tracks user-facing settings and cached battery info |

### 4.5 Migration Interface Rules

- Legacy C modules may temporarily call C-exported functions that forward into C++ service objects.
- New C++ components shall not depend on file-scope mutable globals defined in legacy modules.
- New screen classes may wrap existing LVGL screen creation logic before deeper behavior extraction.

## 5. Acceptance Criteria

- **AC-001**: Given the full refactor plan, when a maintainer reads this specification, then they can identify the target layers, ownership boundaries, and migration constraints without referring to source code.
- **AC-002**: Given a future implementation phase, when a new class is proposed, then it can be mapped to one of the defined layers in Section 4.
- **AC-003**: Given a migration step, when legacy C/C++ interoperability is required, then the step respects the compatibility and C ABI rules defined in this specification.
- **AC-004**: Given review of the target architecture, when mutable runtime state is examined, then the preferred owning object is explicit rather than inferred from file-scope statics.
- **AC-005**: Given a generated implementation plan, when compared against this specification, then the plan preserves the baseline product behavior and the embedded constraints defined here.

## 6. Test Automation Strategy

- **Test Levels**: Architecture review, static code review, phased unit testing where practical, hardware smoke testing when implementations land
- **Frameworks**: Existing repository tooling for code review and CodeQL; future unit frameworks may be added in later implementation phases if justified
- **Test Data Management**: Use deterministic in-memory state fixtures for any future host-side tests of domain logic
- **CI/CD Integration**: Documentation and code review checks can run immediately; firmware build and hardware validation remain environment-dependent
- **Coverage Requirements**: No coverage threshold is imposed by this specification; future domain-level tests should target rules-heavy components first
- **Performance Testing**: Verify event-loop responsiveness and LVGL update cadence during hardware validation for implementation phases

## 7. Rationale & Context

The current firmware concentrates most state, rendering, and interaction logic inside large C modules with file-scope mutable state. That structure works for a compact codebase but makes broader changes risky because hardware wiring, gameplay rules, and UI mutations are tightly coupled.

An object-oriented design is appropriate here because the firmware already has natural long-lived runtime concepts: application bootstrap, game session state, display services, settings services, and UI screens. Converting those concepts into explicit objects clarifies ownership, supports gradual extraction of logic from monolithic files, and reduces the chance of unintended cross-module state mutation.

The architecture deliberately favors composition and explicit wiring rather than deep inheritance. This is better aligned with embedded firmware constraints, easier to reason about for AI-generated changes, and simpler to validate incrementally.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: ESP32-S3 board peripherals - provide ADC, PWM, GPIO, timer, and display/touch interfaces

### Third-Party Services
- **SVC-001**: LVGL - provides embedded UI primitives, event dispatch, and timer handling
- **SVC-002**: ESP panel and touch libraries - provide display and touch controller integration

### Infrastructure Dependencies
- **INF-001**: Arduino runtime on ESP32 - provides setup/loop lifecycle and board support
- **INF-002**: FreeRTOS facilities exposed through ESP32 Arduino - provide delays and critical sections

### Data Dependencies
- **DAT-001**: Battery voltage samples from ADC pin 1 - provide input for filtered battery estimation

### Technology Platform Dependencies
- **PLT-001**: C++ compilation in the Arduino ESP32 environment - required for class-based firmware modules
- **PLT-002**: LVGL 8.4.x compatibility - required because the existing UI code and public docs assume this API surface

### Compliance Dependencies
- **COM-001**: None identified beyond safe embedded resource usage and repository contribution expectations

## 9. Examples & Edge Cases

```text
Example: migrating commander-damage logic

Current state:
- UI callbacks directly mutate commander damage totals and player life totals.

Target state:
- CommanderDamageController receives an action.
- MultiplayerGameState validates and applies the change.
- MultiplayerScreen refreshes visible labels from state.

Edge cases to preserve:
- damage totals must not go below zero
- life totals remain clamped to existing limits
- switching selected player with a pending preview still commits correctly
- battery polling must not mutate UI from interrupt context
```

## 10. Validation Criteria

- The specification is self-contained and describes the target architecture without external assumptions.
- The specification identifies at least the application root, platform service layer, domain state layer, and presentation layer.
- The specification defines migration-compatible constraints for legacy C/C++ coexistence.
- The specification is consistent with current hardware/platform assumptions in repository documentation.
- The specification provides enough structure for subsequent phased implementation planning.

## 11. Related Specifications / Further Reading

- `/home/runner/work/knobby-mtg-life-counter/knobby-mtg-life-counter/docs/specifications/system-spec.md`
- `/home/runner/work/knobby-mtg-life-counter/knobby-mtg-life-counter/docs/specifications/functional-spec.md`
- `/home/runner/work/knobby-mtg-life-counter/knobby-mtg-life-counter/spec/spec-process-cpp-oop-refactor-phases.md`
