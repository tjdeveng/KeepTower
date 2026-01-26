# Compliance Code Audit Plan (CONTRIBUTING + SECURITY)

**Project:** KeepTower

**Last updated:** 2026-01-26

**Purpose:** Provide a repeatable audit plan to ensure the codebase complies with:
- Repository contribution standards (root `CONTRIBUTING.md`)
- Security commitments and practices (root `SECURITY.md`)
- Security review baselines (e.g., `docs/audits/CODE_REVIEW.md`)
- FIPS-140-3 “ready” cryptographic design (e.g., `docs/developer/FIPS_COMPLIANCE.md`)

This document is intentionally actionable: it lists what to check, how to check it, and what “pass” means.

---

## 1. Scope and Audit Gates

### In-scope
- `src/` (core logic, crypto, vault format, I/O, UI)
- `tests/` (unit/integration tests, security tests)
- Build + CI configuration (`meson.build`, `meson_options.txt`, workflows, scripts)
- Security/FIPS docs and implementation coupling

### Out-of-scope (unless explicitly requested)
- Third-party code vendored/downloaded in CI (review only configuration and pinning)
- OS-level packaging beyond project scripts

### Pass/Fail Gates (minimum bar)
- **Security:** no open Critical/High findings in crypto, secrets handling, or file I/O.
- **Memory safety:** ASan/UBSan (and LeakSanitizer if available) show no actionable issues in gating test suites.
- **FIPS-ready mode:** when FIPS mode is enabled, crypto operations are routed through OpenSSL 3.x provider APIs and use approved algorithms only.
- **Maintainability:** critical modules have clear boundaries; no “god objects” in security-critical paths without a remediation ticket.
- **Regression prevention:** audit produces a backlog and at least one CI gate recommendation.

---

## 2. Policy-to-Checklist Mapping

### CONTRIBUTING.md (root)
Focus areas to verify:
- Style: naming, formatting (4 spaces), line length, braces.
- Modern C++: RAII, smart pointers, `std::expected`, `std::span`, avoid `new/delete`.
- SOLID/SRP: classes have a single reason to change.
- Security: input validation, safe error handling, memory clearing.

### SECURITY.md (root)
Security commitments to verify with code evidence/tests:
- `OPENSSL_cleanse()` used for sensitive data clearing.
- `mlock()` used for sensitive buffers (and failures handled safely).
- Clipboard auto-clear (30 seconds).
- Atomic writes, file permissions (0600), backups present.
- Input validation and length limits.

---

## 3. Inventory: Security-Critical Components

Create and maintain a component map (table) for review focus.

Suggested components:
- Vault encryption/decryption (AES-256-GCM)
- Key derivation (PBKDF2-HMAC-SHA256)
- RNG (OpenSSL DRBG / provider RNG)
- Vault file I/O (atomic writes, permissions)
- Backup/restore logic
- Username hash migration paths
- YubiKey integration paths
- Memory locking + secure clearing helpers
- Logging paths (ensure no secrets leak)

Deliverable: `docs/audits/COMPLIANCE_COMPONENT_MAP.md` (or a section in the final report).

---

## 4. Maintainability / Clean Code Review

### What to check
- Large classes with multiple responsibilities (especially “*Manager*” classes).
- High coupling between UI + crypto + persistence.
- Long functions / complex control flow in security-critical code.
- Error handling consistency: prefer `std::expected` patterns (per CONTRIBUTING).

### What to produce
- A short list of hotspots (top 5–10), each with:
  - why it’s a risk
  - minimal refactor suggestion
  - whether it’s blocking or backlog

---

## 5. SRP / SOLID Review (Targeted)

### Approach
- Review critical modules first (crypto + vault I/O + secret handling), then expand.
- Prefer small, safety-driven refactors; avoid churn.

### Signals of SRP issues
- A class that handles crypto + persistence + UI + configuration.
- Methods that do: parse → decrypt → update model → write file → log → notify UI.

Deliverable: remediation tickets and/or a small “architecture delta” note.

---

## 6. FIPS-140-3 Ready Crypto Review

Primary reference: `docs/developer/FIPS_COMPLIANCE.md`.

### What to verify
- OpenSSL 3.x provider architecture is used (`OSSL_PROVIDER_load()` patterns, property queries).
- In FIPS mode, only approved algorithms are reachable:
  - AES-256-GCM
  - PBKDF2-HMAC-SHA256/SHA512 (if used)
  - SHA-256/SHA-512 + HMAC
  - DRBG via provider
- No deprecated low-level crypto APIs are used on FIPS paths.
- FIPS enable/disable behavior is explicit and test-covered.

### Evidence required
- Code references (paths/symbols)
- Test evidence (existing “FIPS Mode Tests” suite)
- Runtime logs are non-sensitive and correctly reflect state

Deliverable: `docs/audits/FIPS_CRYPTO_AUDIT_YYYY-MM-DD.md`.

---

## 7. Sensitive Data Lifecycle & Memory Protection

Primary references:
- `SECURITY.md` expectations
- `docs/audits/CODE_REVIEW.md` findings (secure erasure, clipboard, etc.)

### What to check
- Where secrets live (passwords, derived keys, DEKs/KEKs, plaintext buffers).
- Whether secrets are:
  - zeroized with `OPENSSL_cleanse()` (not just `.clear()`)
  - locked with `mlock()` when appropriate
  - never logged
  - not persisted accidentally
- Destructors / shutdown paths securely wipe.

Deliverable: a “secret lifecycle” diagram + findings list.

---

## 8. Static Analysis

### Tools
- `clang-tidy` (recommended)
- `cppcheck` (optional)

### Output handling
- Define what is blocking vs informational.
- Document suppressions with justification.

Deliverable: `docs/audits/STATIC_ANALYSIS_YYYY-MM-DD.md`.

---

## 9. Memory Leaks & UB (Sanitizers)

### Tools
- ASan + UBSan (and LeakSanitizer when supported)
- Use `asan.supp` if needed and justified

### What to run
- A minimal gating subset first (fast tests), then broaden.

Deliverable: `docs/audits/SANITIZER_RUN_YYYY-MM-DD.md` with commands + outcomes.

---

## 10. Dependency and Build Config Review

### What to check
- OpenSSL version expectations and FIPS provider availability handling.
- Compiler hardening flags (where applicable).
- Warning levels and whether warnings are treated as errors in CI.
- Script safety (no accidental vault deletion, etc.).

Deliverable: short config report + any CI gate proposals.

---

## 11. Test Coverage & Security Tests

### What to ensure exists
- FIPS mode tests (already documented).
- Backup/restore integrity tests.
- Permission/atomic-write tests.
- Clipboard auto-clear behavior (if testable).
- Corruption recovery tests.

Deliverable: list of missing tests + prioritized additions.

---

## 12. Reporting & Remediation Backlog

### Findings format
Each finding should include:
- **Severity:** Critical / High / Medium / Low
- **Category:** SRP, FIPS, memory, crypto, I/O, logging, UI, build
- **Evidence:** file(s), symbol(s), repro steps if applicable
- **Impact:** what can happen, in plain language
- **Fix suggestion:** minimal safe fix
- **Owner + ETA**

### Deliverables
- `docs/audits/COMPLIANCE_AUDIT_REPORT_YYYY-MM-DD.md`
- GitHub issues for each actionable item with labels: `security`, `fips`, `memory`, `srp`, `tech-debt`
- A “CI gates” proposal (what becomes required to merge)

---

## Suggested “Tomorrow Start” Checklist

1. Confirm audit scope and pass/fail gates.
2. Build component map for security-critical code.
3. Run sanitizer build/tests for the gating suites.
4. Start FIPS path review (provider + algorithms + tests).
5. Start sensitive data lifecycle review (secure clear + mlock + logs).

