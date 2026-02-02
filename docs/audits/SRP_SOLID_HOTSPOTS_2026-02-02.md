# SRP/SOLID Hotspot Review

**Project:** KeepTower
**Date:** 2026-02-02
**Scope:** Targeted maintainability review per `docs/developer/COMPLIANCE_CODE_AUDIT_PLAN.md` sections 4–5

## Goal
Identify the highest-risk SRP/SOLID hotspots (especially in security-critical paths), and record **minimal, low-churn refactor tickets** that improve maintainability without destabilizing crypto / vault I/O.

This is intentionally not a “big rewrite” plan.

## Review Method
- Prioritize by responsibility sprawl + coupling and by rough size (LoC).
- Focus on security-critical orchestration paths first (vault lifecycle, auth, migration, crypto and I/O boundaries).

**Top LoC drivers (for prioritization)**
- `src/core/VaultManagerV2.cc` (2451)
- `src/core/VaultManager.cc` (2282)
- `src/ui/dialogs/PreferencesDialog.cc` (2179)
- `src/ui/windows/MainWindow.cc` (2064)
- `src/core/VaultManager.h` (1956)

## Hotspots (Top 10)

Each item includes: *Why it’s risky* → *Minimal refactor suggestion* → *Blocking vs backlog*.

### 1) Vault core orchestration (“god object”)
- **Location:** `src/core/VaultManager.h`, `src/core/VaultManager.cc` (+ `src/core/VaultManagerV2.cc`)
- **Why risky:** One class spans: OpenSSL provider/FIPS state, vault open/close/save, policy, memory-locking, backup behavior, multi-user V2 auth + migration, key wrapping, Reed–Solomon, UI-facing state, and error translation. This increases regression risk and makes security review harder because invariants are distributed across long methods and many members.
- **Minimal refactor suggestion:** Extract narrow internal helpers (initially in `src/core/` as `*.h/.cc` with unit tests) and have `VaultManager` delegate:
  - `FipsProviderManager` (provider load/unload, enable/disable, availability checks)
  - `MemoryLockManager` (platform-specific lock/unlock + rlimit behavior)
  - `VaultBackupPolicy` (backup naming/rotation decisions)
  - `VaultOpenCloseService` (mechanical open/close sequencing that is testable)

**Status:** Backlog (high priority). Not blocking unless it blocks further security work.

### 2) V2 auth + migration logic coupled in one compilation unit
- **Location:** `src/core/VaultManagerV2.cc`
- **Why risky:** Combines authentication, user/session state, keyslot operations, username-hash migration strategy, and assorted “rescue” paths in one file. Migration and authentication are security-sensitive and should be easy to reason about.
- **Minimal refactor suggestion:** Extract focused helpers/classes (even if kept internal to `src/core/`):
  - `UsernameHashMigrationStrategy` (phase 1/2/3 logic)
  - `KeySlotLookup` (find/verify slot without side-effects)
  - `V2AuthService` (auth result building + session init)

**Status:** Backlog (high priority).

### 3) VaultManager header includes too much (coupling amplification)
- **Location:** `src/core/VaultManager.h`
- **Why risky:** Includes many heavy components (`VaultCrypto`, `VaultIO`, serialization, managers, services, orchestrators, YubiKey). This increases build coupling, makes layering unclear, and encourages “reach-through” usage from UI.
- **Minimal refactor suggestion:** Use forward declarations and pimpl/member pointers where possible; move “implementation-only” includes to `.cc`. In particular, consider:
  - Keep `VaultManager.h` as a stable façade.
  - Move orchestrator/service dependencies behind interfaces or private `Impl`.

**Status:** Backlog (medium-high priority).

### 4) MainWindow mixes UI composition with non-trivial application orchestration
- **Location:** `src/ui/windows/MainWindow.cc`
- **Why risky:** Responsible for theme detection, settings monitoring, dialog wiring, vault lifecycle orchestration, controller setup, import/export hooks, etc. This inflates change surface for any feature.
- **Minimal refactor suggestion:** Extract “app shell” responsibilities:
  - `ThemeController` (settings + GNOME monitoring)
  - `VaultUiCoordinator` (open/close/lock/unlock event wiring)
  - Move import/export triggers to a dedicated controller that depends on `VaultIOHandler`

**Status:** Backlog (medium priority).

### 5) PreferencesDialog is a mega-dialog managing policy + persistence + UI state
- **Location:** `src/ui/dialogs/PreferencesDialog.cc`
- **Why risky:** Single class owns UI layout, settings persistence, validation, and some vault-specific toggles (e.g. FIPS mode, RS defaults). Large UI classes trend toward fragile behavior and make it easy to accidentally couple UI to core policy decisions.
- **Minimal refactor suggestion:** Split into tab/page widgets with dedicated presenters:
  - `AppearancePreferencesPage`
  - `SecurityPreferencesPage`
  - `StoragePreferencesPage`
  - Shared `PreferencesPresenter` for reading/writing `Gio::Settings` with validation (keep core unaware of Gtk)

**Status:** Backlog (medium priority).

### 6) YubiKeyManager mixes device discovery, caching, protocol logic, and crypto helpers
- **Location:** `src/core/managers/YubiKeyManager.cc`
- **Why risky:** Large file contains global caching, libfido2 lifecycle, device enumeration, and crypto derivations. Hardware variability and thread-safety concerns magnify the cost of complexity here.
- **Minimal refactor suggestion:** Extract and harden boundaries:
  - `Fido2GlobalInit` (single-responsibility init/shutdown)
  - `YubiKeyDeviceDiscovery` (enumeration + caching policy)
  - `YubiKeyCredentialStore` (cred_id lifecycle and persistence rules)

**Status:** Backlog (medium-high priority).

### 7) UI V2AuthenticationHandler couples dialogs, YubiKey presence checks, core auth, and clipboard policy
- **Location:** `src/ui/managers/V2AuthenticationHandler.cc`
- **Why risky:** UI flow controllers tend to become “mini state machines”; mixing clipboard security policy and device probing here makes behavior harder to test.
- **Minimal refactor suggestion:** Introduce a small “flow state” object and push UI into an interface:
  - `IV2AuthView` (show dialogs, display errors)
  - `V2AuthFlowController` (pure flow logic + sequencing)

**Status:** Backlog (medium priority).

### 8) UI VaultIOHandler mixes UX flow, filesystem import/export, parsing, and error reporting
- **Location:** `src/ui/managers/VaultIOHandler.cc`
- **Why risky:** The import/export flows handle security warnings, authentication gating, filesystem operations and format parsing. This risks subtle security regressions when UI changes.
- **Minimal refactor suggestion:**
  - `ExportFlowController` (dialog sequencing + reauth gate)
  - `ImportFlowController` (format selection + progress/results)
  - Keep parsing in `ImportExport::*` and keep file permission / fsync handling close to where the file is written.

**Status:** Backlog (medium priority).

### 9) ImportExport is large but mostly cohesive; still a hotspot for correctness + edge cases
- **Location:** `src/utils/ImportExport.cc`
- **Why risky:** File-format parsing and plaintext export are sensitive; complexity raises risk of parser bugs and partial/incorrect escaping.
- **Minimal refactor suggestion:** Split by format while keeping the current namespace API stable:
  - `ImportExportCsv.*`, `ImportExportKeepassXml.*`, `ImportExport1Password.*`
  - Add targeted parsing tests around tricky CSV quoting/newlines.

**Status:** Backlog (medium priority).

### 10) Multi-user types/serialization and migrations in one place
- **Location:** `src/core/MultiUserTypes.cc`
- **Why risky:** Parsing/serialization/migration code is security-sensitive (untrusted inputs) and grows quickly.
- **Minimal refactor suggestion:** Split “data model” vs “wire format”:
  - `KeySlot` plain type + invariants
  - `KeySlotSerializer` / `KeySlotDeserializer` with strict bounds helpers

**Status:** Backlog (medium priority).

## Cross-cutting notes (SOLID / policy enforcement)
- **Logging + identifiers:** Several auth/migration paths log usernames at `info` level. Even if usernames aren’t “secrets”, they can be sensitive identifiers in real deployments. Recommend consolidating into a single policy: *avoid logging plaintext usernames except in explicit debug/dev modes*, and centralize formatting/redaction helpers.
- **UI/core boundary:** UI layer currently includes core headers heavily and directly orchestrates flows. Prefer having UI call “services” and treat core as a stable API.

## Remediation Ticket Backlog (Suggested)

These are phrased to map cleanly to GitHub issues later.

1. **Extract FIPS provider manager from VaultManager** (priority: high)
2. **Extract V2 username-hash migration strategy from VaultManagerV2** (priority: high)
3. **Reduce includes/coupling in VaultManager.h via forward declarations / Impl** (priority: medium-high)
4. **Split PreferencesDialog into tab pages + presenter** (priority: medium)
5. **Introduce ThemeController and VaultUiCoordinator for MainWindow** (priority: medium)
6. **Split YubiKeyManager into init/discovery/protocol components** (priority: medium)
7. **Refactor V2AuthenticationHandler into a testable flow controller** (priority: medium)
8. **Refactor import/export UI flow controllers** (priority: medium)
9. **Split ImportExport by format + add edge-case parsing tests** (priority: medium)
10. **Split MultiUserTypes model vs serialization** (priority: medium)

## Non-goals
- No API-breaking refactors in this audit pass.
- No large-scale architectural rewrite.

## “Done” criteria for this deliverable
- Hotspots identified (above)
- Backlog items recorded (above)
- Linked into the main compliance audit report
