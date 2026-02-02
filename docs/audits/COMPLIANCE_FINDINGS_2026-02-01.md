# Compliance Audit Findings

**Project:** KeepTower

**Audit name:** CONTRIBUTING.md + SECURITY.md Compliance Audit

**Date:** 2026-02-01

**Auditor(s):** (fill)

**Scope reviewed:** Phase 3/4 execution evidence (build + targeted security tests)

---

## Finding: UI security tests depended on user dconf state

- **ID:** CA-2026-02-01-001
- **Severity:** Low
- **Category:** tests
- **Status:** Fixed
- **Owner:** (fill)

### Summary
`UI Security Tests` could fail on developer machines if `com.tjdeveng.keeptower` settings had been customized previously (e.g., `clipboard-clear-timeout=5`).

### Policy/Requirement
- CONTRIBUTING.md: tests should be reliable and repeatable.

### Evidence
- **File(s):** `tests/test_ui_security.cc`
- **Notes:** Resets relevant keys during `SetUp()` so defaults are validated independent of persisted user config.

### Fix Verification Checklist
- [x] `meson test -C build --print-errorlogs "UI Security Tests"` passes
- [x] `meson test -C build-asan --print-errorlogs "UI Security Tests"` passes

---

## Finding: Migration Priority 3 test timeout too tight (flake risk)

- **ID:** CA-2026-02-01-002
- **Severity:** Medium
- **Category:** tests
- **Status:** Fixed
- **Owner:** (fill)

### Summary
`Username Hash Migration Priority 3 Tests` can legitimately take slightly over 120 seconds on some machines, causing timeouts in the `migration` suite.

### Policy/Requirement
- CONTRIBUTING.md: CI reliability; tests should not be flaky due to unrealistic timeouts.

### Evidence
- **File(s):** `tests/meson.build`
- **Notes:** Timeout increased from 120s → 180s. Observed runtime ~124s on this machine.

### Fix Verification Checklist
- [x] `meson test -C build --print-errorlogs "Username Hash Migration Priority 3 Tests"` passes within new timeout

---

## Finding: ASan+LSan aborts on OpenSSL provider allocations in FIPS tests

- **ID:** CA-2026-02-01-003
- **Severity:** Medium
- **Category:** tests
- **Status:** Fixed (workaround) / Open (root-cause external)
- **Owner:** (fill)

### Summary
Under ASan with leak detection enabled, `FIPS Mode Tests` abort due to LeakSanitizer reporting allocations attributed to OpenSSL provider/FIPS module code.

### Policy/Requirement
- CONTRIBUTING.md: sanitizer evidence should be runnable/repeatable.
- SECURITY.md: memory safety verification (sanitizers) as part of hardening.

### Evidence
- **File(s):** `tests/meson.build`
- **Notes:** For ASan builds, leak detection is disabled for this specific test binary via `ASAN_OPTIONS=detect_leaks=0...`.

### Recommended Fix (Preferred)
- Keep the per-test leak-disable (current), and add a follow-up task to investigate whether OpenSSL provider unloading/cleanup can be made leak-clean in this environment.

### Follow-up Tasks (Triage)
- [ ] Re-run `FIPS Mode Tests` under ASan with leak detection enabled and capture the leak report (keep as evidence).
- [ ] Evaluate whether an LSan suppression file is appropriate for known OpenSSL provider/FIPS module allocations in this environment.
- [ ] If feasible/safe for this project, evaluate whether provider/module cleanup paths can be invoked at process exit to reduce LSan noise.

### Fix Verification Checklist
- [x] `meson test -C build-asan --print-errorlogs "FIPS Mode Tests"` passes
- [ ] Re-enable leak detection for this test without aborts (follow-up)

---

## Finding: Export flow could dereference null VaultManager

- **ID:** CA-2026-02-01-004
- **Severity:** High
- **Category:** safety
- **Status:** Fixed
- **Owner:** (fill)

### Summary
In the export authentication flow, the password dialog response handler could call `m_vault_manager->verify_credentials(...)` even when `m_vault_manager` was null (reported by clang-analyzer).

### Policy/Requirement
- SECURITY.md: reliability and safety for security-relevant UI flows.

### Evidence
- **File(s):** `src/ui/managers/VaultIOHandler.cc`
- **Notes:** Added an early guard in the response handler to abort export if the vault is not open.

### Fix Verification Checklist
- [x] `clang-tidy` no longer reports `clang-analyzer-core.CallAndMessage` for this path (see `docs/audits/static-analysis/clang-tidy_2026-02-01_after_fixes.txt`)

---

## Finding: Deserialization bounds checks used overflow-prone arithmetic

- **ID:** CA-2026-02-01-005
- **Severity:** Medium
- **Category:** safety
- **Status:** Fixed
- **Owner:** (fill)

### Summary
`KeySlot::deserialize` used `offset + N > size` style checks (with `size_t`) which can overflow. This is low-likelihood in normal use but is a correctness/safety issue for adversarial inputs.

### Policy/Requirement
- SECURITY.md: robust parsing and safe handling of untrusted inputs.

### Evidence
- **File(s):** `src/core/MultiUserTypes.cc`
- **Notes:** Switched to subtraction-based bounds checking and made iterator math explicit.

### Fix Verification Checklist
- [x] `clang-tidy` run completes and no longer reports the prior unused-variable warning in this function.

---

## Finding: Temporary key buffer cleared after move (analyzer signal)

- **ID:** CA-2026-02-01-006
- **Severity:** Low
- **Category:** correctness
- **Status:** Fixed
- **Owner:** (fill)

### Summary
In the non-YubiKey create-vault path, a password-derived key buffer was moved into `m_encryption_key` and then passed to `secure_clear(...)`. While moved-from containers are valid-but-unspecified, this pattern triggers analyzers and can be misread in audits.

### Policy/Requirement
- CONTRIBUTING.md: code should be clear and maintainable.
- SECURITY.md: avoid ambiguous sensitive-data handling patterns.

### Evidence
- **File(s):** `src/core/VaultManager.cc`
- **Notes:** Copy into `m_encryption_key` and then clear the temporary buffer (small fixed-size key; negligible overhead).

---

## Finding: Sensitive data leaked to logs (password preview / usernames)

- **ID:** CA-2026-02-01-009
- **Severity:** High
- **Category:** security
- **Status:** Fixed
- **Owner:** (fill)

### Summary
Some crypto and serialization paths logged sensitive material at `Info` level:

- `KeyWrapping::derive_kek_from_password(...)` logged a hex preview of the password bytes.
- V2 keyslot/vault-save paths logged plaintext usernames while preparing headers for serialization.

This is incompatible with the project’s security posture because logs commonly end up in bug reports, telemetry, or are accessible to other local users.

### Policy/Requirement
- SECURITY.md: avoid leaking secrets (passwords/keys) and limit sensitive identifiers in logs.

### Evidence
- **File(s):**
	- `src/core/KeyWrapping.cc`
	- `src/core/MultiUserTypes.cc`
	- `src/core/VaultManager.cc`

### Fix
- Removed the password preview logging entirely.
- Replaced plaintext-username logging with non-sensitive debug-only summaries.

### Fix Verification Checklist
- [x] `meson test -C build --print-errorlogs "FIPS Mode Tests"` passes

---

## Finding: Uninitialized fixed-size buffers in crypto paths

- **ID:** CA-2026-02-01-007
- **Severity:** Medium
- **Category:** correctness
- **Status:** Fixed
- **Owner:** (fill)

### Summary
Several `std::array<uint8_t, N>` buffers in `VaultManagerV2` were declared without initialization. Although they are typically fully written before use, initializing eliminates any risk of partial-use and reduces audit/static-analysis noise.

### Policy/Requirement
- SECURITY.md: deterministic, safe handling of cryptographic material.

### Evidence
- **File(s):** `src/core/VaultManagerV2.cc`
- **Notes:** Value-initialized the identified buffers with `{}`.

---

## Finding: Static-analysis warnings triaged to backlog (non-blocking)

- **ID:** CA-2026-02-01-008
- **Severity:** Low
- **Category:** static-analysis
- **Status:** Open (triaged backlog)
- **Owner:** (fill)

### Summary
After addressing the highest-signal static-analysis findings (null deref, overflow-prone bounds checks, uninitialized buffers), the remaining `clang-tidy`/`cppcheck` reports are largely refactor/performance/style noise or require deeper invariant documentation.

### Policy/Requirement
- CONTRIBUTING.md: keep technical debt tracked and intentional.
- SECURITY.md: prioritize fixes that directly affect memory safety and robust parsing.

### Evidence
- **File(s):** `docs/audits/static-analysis/clang-tidy_2026-02-01_after_fixes.txt`, `docs/audits/static-analysis/cppcheck-report_2026-02-01.txt`

### Triaged Backlog (Examples)
- Narrowing conversions (`size_t` -> `int`/`ptrdiff_t`) in iterator math and API boundaries.
- Unchecked optional access warnings where invariants exist but are not expressed for analyzers.
- "Easily swappable parameters" and by-value performance suggestions (API ergonomics).
- A small number of tool-reported potential issues that need manual confirmation (e.g., `cppcheck` container bounds and UI pointer-nullness warnings).

### Confirm/Deny Pass (2026-02-02)

- `cppcheck` `containerOutOfBounds` at `src/core/MultiUserTypes.cc:495` (KeySlot::deserialize): **Denied (false positive)**.
	- Rationale: the function guards with `if (offset > data.size() || (data.size() - offset) < 2) return;`, which guarantees `pos = offset` and the subsequent `data[pos++]` plus the lookahead `data[pos]` are in-bounds.
	- The `pos < data.size()` check is therefore redundant, but the code is not out-of-bounds on this path.

- `cppcheck` `autoVariables` at `src/core/VaultManagerV2.cc:1488` (`user_slot->username_salt = new_username_salt;`): **Denied (false positive / tool limitation)**.
	- Rationale: `KeySlot::username_salt` is a value type (`std::array<uint8_t, 16>`) (see `src/core/MultiUserTypes.h`), so assigning from a local `std::array` copies bytes; no address escapes.

- `clang-tidy` `bugprone-exception-escape` at `src/core/VaultManager.cc:156` (`~VaultManager() noexcept`): **Backlog (reviewed; not a confirmed bug)**.
	- Notes: destructor body is wrapped in `try { ... } catch (...) { ... }`, so exceptions should not escape. However it calls `close_vault()` which performs logging and may allocate/throw internally depending on the logging implementation.
	- Follow-up (if we want to reduce risk/noise): provide a no-throw, no-logging cleanup path for destructor use (or otherwise ensure logging used in cleanup cannot throw).

- `clang-tidy` `bugprone-unchecked-optional-access` (multiple sites in `VaultManager`/`VaultManagerV2`): **Backlog (invariant not expressed for analyzers)**.
	- Notes: many of these are guarded by higher-level invariants (e.g., functions only called after V2 header initialization) but the invariant is not consistently enforced at the point of access.
	- Follow-up: add explicit precondition checks/early returns (or debug assertions) in public entrypoints to make the invariant machine-checkable.

- `cppcheck` `nullPointerRedundantCheck` at `src/ui/managers/VaultIOHandler.cc:274` (`touch_dialog->present()`): **Denied (false positive)**.
	- Rationale: `touch_dialog` is assigned immediately before `present()` in the same branch; it is not used when null.

### Fix Verification Checklist
- [ ] Define a consistent triage rubric (must-fix vs backlog) for static-analysis output.
- [ ] Periodically re-run static analysis and ensure no new High/Medium safety findings are introduced.
