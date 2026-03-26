---
title: C++ Object-Oriented Firmware Refactor Phased Implementation Plan
version: 1.0
date_created: 2026-03-26
last_updated: 2026-03-26
owner: jsarje
tags: [process, architecture, firmware, cpp, oop, migration, planning]
---

# Introduction

This specification defines the phased implementation sequence for refactoring the firmware into a C++ object-oriented architecture. It converts the architectural target into a practical rollout plan that can be executed incrementally with low behavioral risk.

## 1. Purpose & Scope

This specification defines the ordered migration phases, their objectives, deliverables, sequencing rules, and completion criteria.

Scope:

- High-confidence migration order
- Phase-by-phase responsibilities and boundaries
- Required outputs from each phase
- Compatibility rules between legacy and new modules
- Review and validation expectations for each phase

Out of scope:

- Exact class member definitions
- Feature expansion unrelated to the refactor
- Hardware verification procedures beyond stating where they are required

Audience:

- Maintainers implementing the staged refactor
- Reviewers approving phase boundaries
- AI agents performing constrained implementation work in future tasks

Assumptions:

- The branch may accept documentation and refactor work without build validation because PlatformIO is not functional in this environment
- The current codebase remains the canonical behavioral baseline during migration

## 2. Definitions

- **Bridge Phase**: A phase that introduces new interfaces or wrappers while leaving legacy behavior largely intact.
- **Extraction Phase**: A phase that moves logic from legacy modules into new object-oriented modules.
- **Legacy Module**: Existing firmware files such as `knob.c` and the display stack that predate the full object-oriented design.
- **Phase Exit Criteria**: Conditions that must be satisfied before the next phase begins.
- **Vertical Slice**: A migration step that includes state, controller, and UI pieces for one feature area.

## 3. Requirements, Constraints & Guidelines

- **REQ-001**: The migration shall proceed in small, reviewable phases with behavior preservation as the default objective.
- **REQ-002**: Each phase shall produce a stable boundary that reduces future coupling even if later phases have not started.
- **REQ-003**: Each phase shall specify what moves, what remains legacy, and what temporary bridge is allowed.
- **REQ-004**: State extraction shall precede broad UI rewrites so logic can be validated independently of screen construction.
- **REQ-005**: Hardware service abstraction shall precede full controller/view separation for features that directly touch board APIs.
- **REQ-006**: No phase shall require a full rewrite of all screens at once.
- **REQ-007**: The migration shall prioritize the multiplayer flow because it is the only in-scope product experience.

- **CON-001**: Build validation is not available in this environment, so phase planning must emphasize behavior-preserving boundaries and manual reviewability.
- **CON-002**: Existing public behavior and hardware assumptions remain authoritative until later specs explicitly change them.
- **CON-003**: Legacy C modules may remain during intermediate phases if ownership boundaries become clearer.
- **CON-004**: Phases shall avoid introducing duplicate competing sources of truth for gameplay state.

- **GUD-001**: Prefer extracting pure or near-pure domain logic before extracting LVGL-heavy view code.
- **GUD-002**: Prefer replacing globals with one owned object at a time rather than many parallel partial containers.
- **GUD-003**: Document temporary bridges explicitly and remove them in a later named phase.
- **GUD-004**: Tie each phase to observable repository artifacts such as new classes, removed globals, or narrowed headers.

## 4. Interfaces & Data Contracts

### 4.1 Phase Inventory

| Phase | Name | Primary Outcome |
| --- | --- | --- |
| 0 | Documentation and boundary alignment | Shared target architecture and phased plan are documented |
| 1 | Application root consolidation | One root app object owns startup and loop orchestration |
| 2 | Platform service extraction | Hardware-facing responsibilities move behind service objects |
| 3 | Session state extraction | Runtime gameplay/settings state becomes explicit model objects |
| 4 | Multiplayer domain controller extraction | Gameplay rules move out of UI-heavy legacy code |
| 5 | Screen/view class extraction | LVGL screen construction and refresh logic move into view classes |
| 6 | Navigation and action dispatch unification | Cross-screen flow becomes controller-driven |
| 7 | Legacy bridge removal | Temporary C ABI shims and redundant globals are removed |
| 8 | Hardening and cleanup | Naming, ownership, and documentation are normalized |

### 4.2 Required Phase Artifact Contract

Each implementation phase should produce:

| Artifact Type | Required Content |
| --- | --- |
| Source changes | New classes or reduced legacy scope aligned to the phase objective |
| Boundary update | Clear ownership transfer from legacy code to new object(s) |
| Documentation update | Phase summary in PR description and related spec links if architecture changed |
| Validation note | Explicit statement that build validation was skipped or performed |

### 4.3 Temporary Bridge Rules

```text
Allowed:
- C-exported wrappers that forward into C++ services
- legacy UI code reading from new state objects through adapters
- controller methods invoked from existing LVGL callbacks

Not allowed:
- two writable state owners for the same gameplay value
- new hardware calls added directly into legacy UI code after services exist
- permanent coexistence of old and new interfaces after the designated cleanup phase
```

## 5. Acceptance Criteria

- **AC-001**: Given this phased plan, when a maintainer starts a migration task, then they can identify the correct next phase and its exit criteria.
- **AC-002**: Given any proposed refactor step, when compared against the phase inventory, then the step can be classified as aligned, premature, or out of scope.
- **AC-003**: Given an intermediate branch state, when reviewers inspect ownership, then they can determine which responsibilities are still legacy and which have been migrated.
- **AC-004**: Given the final migration end state, when all phases are complete, then hardware access, state, and view logic are no longer concentrated in `knob.c`.
- **AC-005**: Given the lack of build validation in this environment, when reviewing a phase PR, then the PR still contains explicit validation notes and clearly bounded behavioral intent.

## 6. Test Automation Strategy

- **Test Levels**: Documentation review for Phase 0, code review and security analysis for all code phases, future host-side unit tests for extracted domain/services, hardware smoke tests for UI and hardware phases
- **Frameworks**: Existing automated code review and CodeQL; future lightweight unit framework may be introduced if justified by extracted pure logic
- **Test Data Management**: Prefer fixture-style player state and commander damage matrices for domain tests
- **CI/CD Integration**: Keep PRs small so review tooling remains effective even without reliable firmware build execution
- **Coverage Requirements**: No mandatory threshold in this plan; prioritize rule-heavy controller/state code for future tests
- **Performance Testing**: Validate loop cadence, encoder responsiveness, and screen refresh behavior on-device during phases that change event processing

## 7. Rationale & Context

The current firmware contains a good functional baseline but large concentrations of responsibility in a few source files. A full rewrite would be risky because gameplay behavior, hardware integration, and LVGL object graphs are intertwined. A phased migration is safer because it creates stable seams incrementally.

This phase order intentionally starts by clarifying ownership at the top of the app, then extracting hardware services and session state, and only later splitting controllers and views. That ordering reduces the chance that UI rewrites accidentally change gameplay semantics or hardware timing behavior.

The plan also recognizes the practical constraint that PlatformIO build validation is not functional in this environment. Therefore each phase must be small, explicitly scoped, and easy to review for logical consistency.

## 8. Dependencies & External Integrations

### External Systems
- **EXT-001**: ESP32-S3 firmware runtime - required for all hardware-interacting phases

### Third-Party Services
- **SVC-001**: LVGL - required for screen extraction and controller/view integration phases
- **SVC-002**: ESP panel and touch libraries - required for display and input service extraction

### Infrastructure Dependencies
- **INF-001**: Arduino setup/loop lifecycle - required for application root integration
- **INF-002**: Repository PR workflow - required to document phase completion and review notes

### Data Dependencies
- **DAT-001**: Existing gameplay defaults and UI semantics - must be preserved as behavioral baseline inputs to the migration

### Technology Platform Dependencies
- **PLT-001**: C and C++ mixed compilation support - required during bridge phases
- **PLT-002**: Existing source layout under `knobby/` - provides the starting point for extractions and compatibility wrappers

### Compliance Dependencies
- **COM-001**: None beyond repository review and security scanning expectations

## 9. Examples & Edge Cases

```text
Recommended execution sequence

Phase 1:
- Introduce Application class
- Keep existing knob and screen code mostly intact

Phase 3:
- Move multiplayer player names, life totals, pending delta, and brightness into explicit state objects
- Legacy UI continues to read/write through adapters during the transition

Phase 5:
- Extract SettingsScreen first because it has simpler navigation and clearer service boundaries
- Extract multiplayer overview after state/controller seams are stable

Edge cases
- Do not extract a screen class before the state it renders has a stable owner
- Do not create a new controller that still depends on raw file-scope statics in two modules
- Do not remove C wrappers until all legacy C call sites are migrated
```

## 10. Validation Criteria

- The plan defines an ordered set of phases from current architecture to target architecture.
- Each phase has a distinct objective that can be described in a pull request without ambiguity.
- The plan describes both allowable temporary bridges and forbidden transitional states.
- The plan remains consistent with the current multiplayer-only product scope.
- The plan is detailed enough for future implementation tasks to target one phase at a time.

## 11. Related Specifications / Further Reading

- `spec/spec-architecture-cpp-oop-refactor-plan.md`
- `docs/specifications/system-spec.md`
- `docs/specifications/functional-spec.md`

## Appendix A: Detailed Phased Implementation Steps

### Phase 0 — Documentation and boundary alignment

1. Record the target architecture and phased plan in repository specifications.
2. Confirm the current behavioral baseline from the existing system and functional specifications.
3. Ensure the PR description explains that the branch now includes architecture planning artifacts in addition to code refactor work.

**Phase exit criteria**

- Both specifications exist in `/spec/`
- The PR description references the planning scope

### Phase 1 — Application root consolidation

1. Introduce an `Application` class that owns bootstrap and loop progression.
2. Move `knobby_app.cpp` responsibilities into methods on the application root.
3. Keep `knobby.ino` as a delegate-only entrypoint.
4. Preserve the minimum bridging needed for legacy C code such as battery sampling access.

**Phase exit criteria**

- Startup and loop orchestration are owned by one C++ root object
- No additional startup logic remains in `knobby.ino`

### Phase 2 — Platform service extraction

1. Extract battery measurement into a dedicated service class.
2. Extract brightness/backlight control into a dedicated service class.
3. Extract display/touch/encoder initialization ownership into platform service classes or a composed display subsystem.
4. Replace direct hardware access in unrelated modules with service calls.

**Phase exit criteria**

- Hardware APIs are no longer scattered across app and UI modules
- New hardware-facing logic enters through service boundaries only

### Phase 3 — Session state extraction

1. Create `SettingsState` for brightness and battery presentation state.
2. Create `MultiplayerGameState` for player names, life totals, selected player, previews, and commander damage.
3. Remove duplicate writable state from file-scope statics as ownership transfers.
4. Provide temporary adapters for legacy functions that still expect primitive globals.

**Phase exit criteria**

- One explicit owner exists for each gameplay and settings value
- State can be inspected without reading scattered file-scope globals

### Phase 4 — Multiplayer domain controller extraction

1. Move life-total adjustment rules into a multiplayer controller or service.
2. Move commander-damage application rules into a dedicated controller/service.
3. Move reset semantics and selection change handling into domain/controller logic.
4. Keep LVGL callbacks thin by forwarding actions to controller methods.

**Phase exit criteria**

- Business rules are not embedded primarily in widget callbacks
- Controller logic can be reasoned about independently of screen layout

### Phase 5 — Screen/view class extraction

1. Extract the settings screen into a class with `create` and `refresh` responsibilities.
2. Extract the intro screen into a dedicated screen/view class.
3. Extract multiplayer overview and menus into screen classes after state/controller seams are stable.
4. Group screen-local widgets inside screen classes instead of global widget arrays where practical.

**Phase exit criteria**

- Screen construction and refresh code is organized by screen class
- Widget ownership is primarily screen-local, not file-global

### Phase 6 — Navigation and action dispatch unification

1. Introduce a navigation coordinator or UI controller to manage screen transitions.
2. Replace direct ad hoc `lv_scr_load(...)` decisions with centralized flow decisions where practical.
3. Standardize how touch, knob, and timer actions are translated into application actions.

**Phase exit criteria**

- Flow changes are coordinated from one navigation boundary
- Cross-screen transitions do not depend on scattered state checks

### Phase 7 — Legacy bridge removal

1. Remove temporary C wrappers that are no longer needed.
2. Retire compatibility adapters and dead globals.
3. Narrow headers to pure public interfaces.
4. Convert remaining legacy C-heavy modules to C++ only when their dependencies have already migrated.

**Phase exit criteria**

- Compatibility bridges are limited to truly necessary boundaries
- Legacy file-scope mutable state is substantially reduced or eliminated

### Phase 8 — Hardening and cleanup

1. Normalize naming across services, controllers, models, and screens.
2. Update repository docs to reflect the final runtime architecture.
3. Audit ownership, lifetimes, and header dependencies.
4. Perform a final cleanup pass for obsolete comments, wrappers, and transitional files.

**Phase exit criteria**

- The architecture is coherent, documented, and free of known transitional scaffolding
- Remaining files match the final intended C++ object-oriented structure
