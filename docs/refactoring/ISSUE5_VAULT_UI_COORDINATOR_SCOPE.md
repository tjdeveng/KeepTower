# Issue #5: VaultUiCoordinator — Scope & API Boundary

Date: 2026-02-12

## Goal
Reduce `MainWindow` responsibility by introducing a `VaultUiCoordinator` that owns the *vault lifecycle orchestration* (open completion, save, close, and session/menu synchronization) and centralizes wiring between existing UI managers/handlers.

This is intentionally **coordination only**: it should not implement vault crypto/IO (that stays in `VaultManager` / core services) and it should not own widget layout.

## Current Situation (Why)
`MainWindow` already delegates many actions to extracted handlers/managers (`VaultOpenHandler`, `AutoLockHandler`, `VaultIOHandler`, `VaultUiStateApplier` (formerly `UIStateManager`), `MenuManager`, etc.), but the *glue* is still spread across `MainWindow`:

- Vault save/close orchestration lives directly in `MainWindow::on_save_vault()` / `MainWindow::on_close_vault()`.
- The “open completion” flow (`complete_vault_opening()`, `update_session_display()`, `update_menu_for_role()`) is still window-owned.
- Multiple sources of truth exist for state (`m_vault_open`, `m_is_locked`, `m_current_vault_path`, `m_cached_master_password`, plus widget sensitivity/session/status UI state applied by `VaultUiStateApplier`).
- Wiring in the `MainWindow` constructor is long and hard to reason about.

## In Scope (VaultUiCoordinator owns)
### 1) Vault lifecycle orchestration
- **Open completion:** the sequence that occurs after authentication succeeds:
  - `VaultUiStateApplier::set_vault_opened(...)`
  - update `MainWindow` state cache (initially via references; later remove duplicates)
  - repository/service initialization
  - UI refresh (`update_session_display`, account list refresh, tag dropdown, undo/redo state)
  - begin activity monitoring / user activity reset
  - menu role update (`MenuManager::update_menu_for_role(...)`)

- **Save:**
  - validate/persist current account edits (via callback or existing method)
  - `VaultManager::save_vault()`
  - status update / error presentation policy

- **Close:** the end-to-end shutdown pipeline:
  - clipboard clear + auto-lock stop
  - disconnect any vault-specific signal connections (as needed)
  - scrub cached password
  - save current account
  - prompt-save-if-modified
  - `VaultManager::close_vault()`
  - clear undo history
  - reset repositories/services
  - `VaultUiStateApplier::set_vault_closed()`
  - update menus
  - clear widgets (account tree + detail panel)

### 2) Session/menu synchronization (V2)
- Provide one place that computes `is_v2`, `is_admin`, and ensures the menu and session label stay consistent.

### 3) Centralized wiring (later steps)
- Own and wire the existing handler/manager objects that relate to vault lifecycle (see “Not in scope” for what stays elsewhere).

## Out of Scope (stays in MainWindow or existing classes)
- Widget ownership/layout (header bar, split panes, constructing widgets).
- Account/group UI behavior (selection, filtering, rendering) already handled by widgets/controllers.
- Core vault operations, crypto, serialization.
- Theme (handled by `ThemeController`).
- Reworking dialog UX (e.g., rewriting prompt-save to a fully async flow) — coordinator may call existing dialog patterns, but UX should not change in this issue.

## Proposed Minimal Public API
These are the entrypoints MainWindow should delegate to.

- `void on_new_vault();` (delegates to `VaultOpenHandler`)
- `void on_open_vault();` (delegates to `VaultOpenHandler`)
- `void on_save_vault();`
- `void on_close_vault();`
- `void handle_v2_vault_open(const std::string& vault_path);`
- `void complete_vault_opening(const std::string& vault_path, const std::string& username);`

Optional additions depending on what we migrate next:
- `void on_migrate_v1_to_v2();`
- `void update_menu_for_role();`
- `void update_session_display();`

## Dependencies (initial shape)
Keep dependencies explicit and keep ownership clear.

- Required dependencies:
  - `Gtk::Window&` / `Gtk::ApplicationWindow&` (for dialog transience)
  - `VaultManager*`
  - `UI::VaultUiStateApplier*`, `UI::MenuManager*`, `UI::DialogManager*`
- Required callbacks (to avoid moving widget ownership too early):
  - `save_current_account()`
  - `initialize_repositories()` / `reset_repositories()` (+ services equivalents if needed)
  - `update_account_list()` / `update_tag_filter_dropdown()`
  - `clear_account_details()`
  - `account_tree_widget->set_data({}, {})` (encapsulate as a callback)

## State Ownership Plan (incremental)
- **Step 6–8:** coordinator may temporarily operate on the existing `MainWindow` state refs (e.g., `bool& m_vault_open`) to avoid a huge first diff.
- **Step 9:** remove duplicated window state by consolidating on one source of truth (prefer coordinator-owned state; use `VaultUiStateApplier` for widget mutation only).

## Acceptance Criteria for this Issue
- `MainWindow` no longer contains theme monitoring (done) nor vault lifecycle glue (save/close/open completion migrated).
- The observable UX and behavior remains the same (no new dialogs/flows).
- Tests: add a focused unit test for coordinator only if it can be meaningfully headless; otherwise rely on existing integration coverage and keep changes narrow.
