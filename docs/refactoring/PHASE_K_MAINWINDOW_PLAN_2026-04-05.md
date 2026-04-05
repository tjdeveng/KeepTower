# Phase K Plan: MainWindow Decomposition

Date: 2026-04-05
Status: Closed

## Objective
Complete the next architecture modernization phase by reducing `MainWindow` orchestration and UI glue complexity while preserving behavior.

This phase follows Phase J and uses the existing extracted infrastructure (`VaultUiCoordinator`, handlers, managers, controllers) as the baseline.

## Baseline
- `MainWindow.cc` is still a hotspot at ~1853 lines.
- Vault lifecycle entrypoints are already delegated to `VaultUiCoordinator`.
- Remaining concentration is in event glue and widget-level orchestration:
  - selection/detail sync
  - context menu plumbing
  - account/group reorder callbacks
  - repeated vault-open guards and status updates

## Scope
### In Scope
- Further decomposition of `MainWindow` UI orchestration into focused collaborators.
- Removal of repeated guard/boilerplate logic from event handlers.
- Preservation of current UX and callback behavior.

### Out of Scope
- Core vault algorithm changes.
- New UI flows or dialog behavior changes.
- Rewriting existing managers that are already stable.

## Proposed Slice Plan

### K.1 Guard + Action Precondition Consolidation
Goal: Remove repeated `vault_open` and selected-index checks from handlers.

Changes:
- Introduce a small helper utility (or private helper methods) used by action handlers for:
  - vault-open precondition checks
  - selected-account precondition checks
  - standardized status/error reporting on precondition failure
- Migrate high-traffic handlers first:
  - `on_add_account`
  - `on_delete_account`
  - `on_import_from_csv`
  - `on_export_to_csv`
  - `on_generate_password`
  - `on_undo` / `on_redo`

Validation:
- Build compile
- `UI Features Tests`
- `Input Validation Tests`
- `VaultManager Tests`

Commit target:
- `refactor(ui): consolidate MainWindow action preconditions`

### K.2 Selection/Details Synchronization Extraction
Goal: Move selection-to-detail orchestration out of `MainWindow` method bodies.

Changes:
- Extract selection handling + detail rendering orchestration into a focused UI coordinator/handler class.
- Delegate:
  - `on_selection_changed`
  - `display_account_details`
  - `clear_account_details`
- Keep widget ownership in `MainWindow`; extract behavior only.

Validation:
- Build compile
- `UI Features Tests`
- `Settings Validator Tests`

Commit target:
- `refactor(ui): extract selection/detail orchestration from MainWindow`

### K.3 Context Menu + Group Interaction Extraction
Goal: Isolate account/group context menu glue and reorder callbacks.

Changes:
- Extract logic from:
  - `show_account_context_menu`
  - `show_group_context_menu`
  - `on_account_reordered`
  - `on_group_reordered`
  - group action wrappers (`on_create_group`, `on_rename_group`, `on_delete_group`)
- Reduce lambda complexity in constructor signal wiring.

Validation:
- Build compile
- `Group` and account interaction related tests (existing UI/integration set)
- `VaultManager Tests`

Commit target:
- `refactor(ui): extract context menu and group interaction glue`

### K.4 Constructor Wiring Reduction (Composition Cleanup)
Goal: Make construction/wiring readable and maintainable.

Changes:
- Split constructor setup into private setup methods:
  - widget setup
  - action wiring
  - manager/controller creation
  - signal connections
- Keep behavior identical.

Validation:
- Build compile
- full targeted UI/regression subset used in previous slices

Commit target:
- `refactor(ui): split MainWindow construction and wiring steps`

### K.5 Phase K Stabilization + Docs
Goal: Close the phase with regression confidence and architecture docs updates.

Changes:
- Run broad test sweep covering V2 auth, migration, settings, manager flows.
- Update architecture notes in `MainWindow.h` and changelog/roadmap entries.
- Regenerate doxygen and verify warning policy behavior matches project configuration.

Validation:
- Broad suite used at Phase J closeout plus MainWindow-adjacent tests.

Commit target:
- `docs(changelog): close out Phase K`

## Closeout Summary

### Completed Work
- K.1 completed via consolidated action precondition helpers for vault-open and selected-account flows.
- K.2 completed via `AccountSelectionCoordinator` extraction for selection/detail synchronization.
- K.3 completed via `AccountTreeInteractionCoordinator` extraction for context menu and group interaction glue.
- K.4 completed via constructor setup decomposition into focused initialization and wiring helpers.
- K.5 completed with targeted stabilization, regression analysis, and documentation updates.

### Final MainWindow Size
- `src/ui/windows/MainWindow.cc`: 1821 lines
- `src/ui/windows/MainWindow.h`: 420 lines

### Stabilization Notes
- A runtime regression surfaced during K.4 review: `open vault -> select grouped account -> Add Account` could segfault.
- Root cause was a re-entrant selection path in `AccountTreeWidget::select_account_by_id()`:
  - selection callbacks could synchronously rebuild tree rows
  - the method then touched a stale row pointer after callback delivery
- Fix landed in commit `f752920` by re-resolving the selected row after signal delivery instead of retaining row pointers across re-entrant callbacks.
- Added regression coverage in `tests/test_account_tree_widget.cc` for synchronous rebuild-during-selection behavior.

### Validation Performed
- Manual smoke validation:
  - original grouped-account add workflow reproduced and re-checked after fix
  - additional manual group/account creation exercised the repaired path
- Automated validation:
  - `meson compile -C build`
  - `meson test -C build account_tree_widget --print-errorlogs`
  - `meson test -C build 'UI Features Tests' 'Settings Validator Tests' 'VaultManager Tests' 'V2AuthService Unit Tests' 'Username Hash Migration Tests' 'Username Hash Migration Priority 2 Tests' --print-errorlogs`
  - `meson test -C build --suite migration --print-errorlogs`

### Outcome
- Constructor composition cleanup is complete.
- The phase exited with no known blocking regressions in the validated MainWindow-adjacent flows.
- The highest-risk finding from the phase was callback re-entrancy, not constructor extraction itself.

## Exit Criteria
- `MainWindow.cc` reduced with measurable responsibility shift from orchestration methods into dedicated collaborators.
- No behavior regressions in UI and V2 flows.
- Updated architecture docs and roadmap/changelog entries.

## Risk Notes
- Signal lifetime bugs during extraction: keep ownership in `MainWindow`, move behavior only.
- Hidden coupling in callbacks: migrate in narrow slices with immediate compile + tests.
- Avoid broad renames during decomposition slices to keep diffs reviewable.

## Phase Retrospective
- The phase plan was effective when executed in narrow slices with immediate validation and commit boundaries.
- The main stabilization lesson is that UI callback extraction around GTK widgets must assume synchronous re-entrancy until proven otherwise.
