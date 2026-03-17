---
name: Audit bounds checks (vault-first)
about: Vault parsing hardening now, broader parsing audit later
title: "Audit bounds checks: vault parsing hardening (Phase 1) + broader parsing audit (Phase 2)"
labels: ["security", "hardening", "tech debt"]
---

We’ve already fixed a real class of bounds-check issues in vault parsing caused by overflow/underflow-prone arithmetic (e.g., `offset + len <= size`) and type-limit traps (e.g., `uint8_t` compared to `<= 255`). Because these patterns often occur in untrusted-input parsing code, we should proactively audit and standardize on safer “remaining-bytes” checks.

## Problem
Some parsing code uses arithmetic patterns that can:
- Overflow: `offset + needed` can wrap on large inputs
- Underflow: `size - offset` can underflow if `offset > size`
- Hide bugs via always-true comparisons when using small integer types

These issues can lead to incorrect parsing, potential security risk, and/or compiler warnings.

## Preferred safe idiom
- Use: `if (pos > data.size() || (data.size() - pos) < needed) { /* fail */ }`
- Avoid: `if (pos + needed > data.size()) { /* fail */ }`
- Widen length fields early to `size_t` before arithmetic.

---

## Phase 1 — Vault Parsing Audit (Priority / Now)

### Scope
Focus only on vault-related parsing and serialization/deserialization:
- Vault formats (all versions): header parsing, metadata, length-prefixed fields
- Vault manager and services that read/write vault bytes
- Multi-user key slot serialization/deserialization and vault metadata structures

Suggested directories:
- `src/core` (vault format + manager + serialization)
- `src/vault*` (vault IO/crypto/serialization modules, if present)

### Work items
- Identify candidates:
  - `offset/pos + len` comparisons against `.size()`
  - `.size() - pos/offset` without an `offset <= size` guard
  - Length fields stored in small integer types used in arithmetic
  - `<= 255` / `< 256` checks on byte-sized values
- Update code to use remaining-bytes checks (minimal churn; no refactors unless necessary).
- Ensure errors/failures return consistent parsing errors (e.g., corrupted file) where appropriate.

### Suggested command list (vault-only)
- `rg -n --hidden --glob '!.git/**' '\\b(offset|pos)\\b\\s*\\+\\s*[^;]{1,80}\\s*(<=|<|>|>=)\\s*[^;]{0,40}\\.size\\(\\)' src/core src/vault*`
- `rg -n --hidden --glob '!.git/**' '\\.(size)\\(\\)\\s*-\\s*(offset|pos)\\b' src/core src/vault*`
- `rg -n --hidden --glob '!.git/**' '\\buint8_t\\b.*\\b(len|length|size)\\b' src/core src/vault*`
- `rg -n --hidden --glob '!.git/**' '\\b<=\\s*255\\b|\\b<\\s*256\\b' src/core src/vault*`

(Optional report file)
- `mkdir -p /tmp/keeptower-audit`
- `rg -n --hidden --glob '!.git/**' '\\b(offset|pos)\\b\\s*\\+|\\.(size)\\(\\)\\s*-\\s*(offset|pos)\\b|\\buint8_t\\b.*\\b(len|length|size)\\b|<=\\s*255\\b' src/core src/vault* > /tmp/keeptower-audit/vault_bounds_candidates.txt`

### Testing / validation
- Run vault-focused test subset (names may vary by build system):
  - Vault format unit tests (e.g., V2 header parsing)
  - Vault manager tests
  - Vault file service tests
  - Vault YubiKey service tests (if relevant to metadata parsing)
- Ensure no new warnings introduced.

### Acceptance criteria (Phase 1)
- All identified vault parsing candidates are either:
  - Fixed to safe remaining-bytes checks, or
  - Explicitly documented in this issue as “safe as-is” with rationale.
- Vault-related unit tests pass.
- No new parsing regressions (existing vaults continue to load where applicable).

---

## Phase 2 — Broader Parsing Audit (Planned / Later)

### Out of scope for Phase 1
No changes outside vault parsing in Phase 1 unless a fix is trivial and clearly related.

### Scope (Phase 2)
Expand audit to the rest of the codebase where untrusted or semi-trusted data is parsed:
- Imports/exports, migrations, backup/restore
- Any binary or length-prefixed parsing in services/modules outside vault
- Tests/fixtures that deserialize blobs (to prevent future copy/paste of unsafe patterns)
- CLI/UI-adjacent parsing paths if they handle external data

### Work items (Phase 2)
- Repeat the same pattern scan repo-wide.
- Decide whether to introduce a shared helper for bounds checking (only if it reduces repetition without creating dependency tangles).
- Add targeted tests for newly-hardened parsing edges if needed.

### Acceptance criteria (Phase 2)
- Same safety idioms adopted consistently across non-vault parsing code.
- No new warnings across supported toolchains/CI.
- Relevant tests added/updated only where coverage gaps exist.

---

## Notes
This is a hardening task aimed at preventing subtle bugs and security issues, with intentionally minimal behavioral change beyond rejecting malformed/truncated inputs safely.
