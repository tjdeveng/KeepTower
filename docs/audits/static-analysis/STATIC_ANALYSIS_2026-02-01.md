# Static Analysis Evidence

**Project:** KeepTower

**Date:** 2026-02-01

**Build context:** Meson build directory `build/` (uses `build/compile_commands.json`).

## Tools

- `cppcheck` (local): 2.18.3
- `clang-tidy` (local): LLVM 20.1.8

## Commands executed

### cppcheck

- Scope: `src/`
- Raw output saved to: `docs/audits/static-analysis/cppcheck-report_2026-02-01.txt`

### clang-tidy

- Scope: security-critical subset
  - `src/core/VaultManager.cc`
  - `src/core/VaultManagerV2.cc`
  - `src/core/MultiUserTypes.cc`
  - `src/core/io/VaultIO.cc`
  - `src/core/crypto/VaultCrypto.cc`
  - `src/core/services/VaultCryptoService.cc`
  - `src/core/services/VaultFileService.cc`
  - `src/ui/managers/VaultIOHandler.cc`
- Checks enabled:
  - `bugprone-*`, `cert-*`, `security-*`, `performance-*`, `cppcoreguidelines-pro-type-member-init`
- Raw output saved to:
  - `docs/audits/static-analysis/clang-tidy_2026-02-01.txt`

### clang-tidy (post-fix confirmation)

- Scope: files patched based on findings
- Raw output saved to:
  - `docs/audits/static-analysis/clang-tidy_2026-02-01_after_fixes.txt`

## Key findings and actions

### Fixed

- **UI null-deref risk (export auth flow):** `VaultIOHandler` could dereference `m_vault_manager` when null; added early guard.
- **Deserialization robustness:** `KeySlot::deserialize` used `offset + 2 > size` pattern that can overflow; replaced with subtraction-based bounds checking and made iterator math explicit.
- **Key handling correctness/analyzer signal:** avoid cleansing a moved-from vector by copying the derived key into `m_encryption_key` before clearing the temporary.
- **Uninitialized fixed-size buffers:** value-initialized multiple `std::array` buffers in `VaultManagerV2`.
- **Exception spec mismatch:** aligned `VaultManager` destructor declaration with its `noexcept` definition.

These are tracked as findings in `docs/audits/COMPLIANCE_FINDINGS_2026-02-01.md`.

### Still outstanding (triage backlog)

Triage status is tracked in `docs/audits/COMPLIANCE_FINDINGS_2026-02-01.md` (see CA-2026-02-01-008).

**Fix soon (safety/correctness if confirmed)**
- Tool-reported potential bounds/nullness issues that warrant manual confirmation.

**Confirm/Deny pass (2026-02-02)**
- Reviewed the `cppcheck` and `clang-tidy` high-signal warnings and recorded outcomes under CA-2026-02-01-008 in `docs/audits/COMPLIANCE_FINDINGS_2026-02-01.md`.
- `cppcheck` `containerOutOfBounds` in `KeySlot::deserialize`: denied as false positive (guarded by earlier bounds check).
- `cppcheck` `autoVariables` in username salt migration: denied as false positive (`std::array` value assignment).
- `cppcheck` `nullPointerRedundantCheck` for `touch_dialog`: denied as false positive (assigned immediately before use).
- `clang-tidy` exception-escape and unchecked-optional-access warnings: retained as backlog items (invariant/no-throw guarantees not consistently expressed for analyzers).

**Backlog (non-blocking)**
- **Narrowing conversions** in iterator arithmetic and API boundaries (e.g., `size_t` -> `int`/`ptrdiff_t`).
- **Unchecked optional access** patterns (invariants may exist, but currently not enforced in-code for analyzers).
- Bugprone "easily swappable parameters" and performance-by-value suggestions (mostly refactor/ergonomics).

## Notes

- `clang-tidy` reports a very large number of suppressed warnings originating from non-project headers; raw outputs above capture the relevant user-code diagnostics that were emitted.
