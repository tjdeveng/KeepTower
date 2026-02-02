# SRP/SOLID GitHub Issue Drafts

**Project:** KeepTower
**Date:** 2026-02-02
**Source:** `docs/audits/SRP_SOLID_HOTSPOTS_2026-02-02.md`

These drafts are intended to be created as GitHub issues with labels:
- `maintainability`
- `srp`
- `security`
- `audit`

---

## 1) Extract FIPS provider manager from VaultManager

**Title:** `[REFACTOR] Extract FipsProviderManager from VaultManager`

**Body:**
- **Hotspot:** `src/core/VaultManager.cc` / `src/core/VaultManager.h`
- **Problem:** FIPS/provider lifecycle and runtime enable/disable logic is mixed into the main vault orchestration class.
- **Goal:** Isolate OpenSSL provider/FIPS state transitions into a single, testable component.

**Proposed minimal change**
- Introduce `src/core/crypto/FipsProviderManager.{h,cc}` (or similar).
- Keep public API stable; `VaultManager::initialize_fips_mode()` and `VaultManager::set_fips_mode(bool)` delegate to the new helper.

**Acceptance criteria**
- No behavior change other than improved structure.
- Existing `"FIPS Mode Tests"` still pass.
- No new sensitive logs.

**Test plan**
- `meson test -C build --print-errorlogs "FIPS Mode Tests"`

---

## 2) Extract V2 username-hash migration strategy from VaultManagerV2

**Title:** `[REFACTOR] Extract UsernameHashMigrationStrategy from VaultManagerV2`

**Body:**
- **Hotspot:** `src/core/VaultManagerV2.cc`
- **Problem:** Authentication + migration strategy + “rescue” sweeps are interleaved, making it hard to reason about security invariants.
- **Goal:** Make migration logic explicit, unit-testable, and side-effect controlled.

**Proposed minimal change**
- Introduce `src/core/services/UsernameHashMigrationStrategy.{h,cc}`.
- Make slot lookup functions side-effect free (return result struct; caller applies mutations).

**Acceptance criteria**
- Migration suite behavior unchanged.
- No plaintext username logging at `info` level in the migration path.

**Test plan**
- `meson test -C build --print-errorlogs --suite migration`

---

## 3) Reduce includes/coupling in VaultManager.h (forward decl / Impl)

**Title:** `[REFACTOR] Reduce include coupling in VaultManager.h (forward decl / Impl)`

**Body:**
- **Hotspot:** `src/core/VaultManager.h`
- **Problem:** Header pulls in many heavy dependencies (crypto, I/O, orchestrators, services, YubiKey), increasing coupling and rebuild times.
- **Goal:** Treat `VaultManager` as a façade: implementation details move to `.cc` or an internal `Impl`.

**Acceptance criteria**
- `VaultManager.h` includes materially reduced.
- No public API break.
- Build still succeeds in normal + ASan builds.

---

## 4) Split PreferencesDialog into tab pages + presenter

**Title:** `[REFACTOR] Split PreferencesDialog into pages + PreferencesPresenter`

**Body:**
- **Hotspot:** `src/ui/dialogs/PreferencesDialog.cc`
- **Problem:** One class owns UI layout + settings persistence + validation + vault policy toggles.
- **Goal:** Reduce UI change blast radius and make settings logic testable.

**Proposed minimal change**
- Extract `AppearancePreferencesPage`, `SecurityPreferencesPage`, `StoragePreferencesPage` widgets.
- Add `PreferencesPresenter` (GSettings read/write + validation), with no Gtk dependencies where possible.

**Test plan**
- `meson test -C build --print-errorlogs "UI Security Tests"`

---

## 5) Introduce ThemeController and VaultUiCoordinator for MainWindow

**Title:** `[REFACTOR] Extract ThemeController and VaultUiCoordinator from MainWindow`

**Body:**
- **Hotspot:** `src/ui/windows/MainWindow.cc`
- **Problem:** Main window mixes theme plumbing, dialog wiring, and vault lifecycle orchestration.
- **Goal:** Improve SRP and reduce accidental coupling between UI composition and application flow.

**Test plan**
- `meson test -C build --print-errorlogs "UI Security Tests"`

---

## 6) Split YubiKeyManager into init/discovery/protocol components

**Title:** `[REFACTOR] Split YubiKeyManager into focused components (init/discovery/protocol)`

**Body:**
- **Hotspot:** `src/core/managers/YubiKeyManager.cc`
- **Problem:** libfido2 lifecycle, discovery caching, protocol operations, and crypto helpers are mixed together.
- **Goal:** Improve testability and reduce thread-safety risk.

---

## 7) Refactor V2AuthenticationHandler into a testable flow controller

**Title:** `[REFACTOR] Refactor V2AuthenticationHandler into a testable flow controller`

**Body:**
- **Hotspot:** `src/ui/managers/V2AuthenticationHandler.cc`
- **Problem:** Dialog sequencing + device probing + core auth calls + clipboard policy are coupled.
- **Goal:** Make auth flow logic testable and keep UI as a thin view.

---

## 8) Refactor import/export UI flow controllers

**Title:** `[REFACTOR] Extract ImportFlowController / ExportFlowController from VaultIOHandler`

**Body:**
- **Hotspot:** `src/ui/managers/VaultIOHandler.cc`
- **Problem:** UI flow, re-auth gating, filesystem operations, and parsing are mixed.
- **Goal:** Keep security-critical export gating explicit and reduce UI regressions.

---

## 9) Split ImportExport by format + add edge-case parsing tests

**Title:** `[REFACTOR] Split ImportExport by format; add CSV edge-case tests`

**Body:**
- **Hotspot:** `src/utils/ImportExport.cc`
- **Problem:** Large multi-format parser; subtle CSV quoting/newline edge cases are easy to regress.
- **Goal:** Improve cohesion and test coverage without API break.

---

## 10) Split MultiUserTypes model vs serialization

**Title:** `[REFACTOR] Split MultiUserTypes model vs serializer/deserializer`

**Body:**
- **Hotspot:** `src/core/MultiUserTypes.cc`
- **Problem:** Model + wire-format parsing/serialization + migration helpers live together.
- **Goal:** Make parsing boundaries explicit and easier to audit.
