# Compliance Audit Report Template (CONTRIBUTING + SECURITY)

**Project:** KeepTower

**Date:** YYYY-MM-DD

**Auditor(s):**

**Audit scope:**
- Code: (folders/modules)
- Build/CI: (what reviewed)
- Tests: (what executed)

---

## 1. Executive Summary

- **Overall status:** Pass | Conditional Pass | Fail
- **Blocking findings:** (# Critical/High)
- **Non-blocking findings:** (# Medium/Low)
- **Top risks:** (3 bullets)

---

## 2. Scope and Method

### In-scope
-

### Out-of-scope
-

### Methods Used
- Manual review (SRP/SOLID)
- FIPS-ready crypto review (provider + algorithms)
- Sensitive data lifecycle review
- Static analysis (clang-tidy/cppcheck)
- Sanitizers (ASan/UBSan/LSan)

---

## 3. Policy Compliance Checklist

Use this section as a *traceability matrix*: each checkbox should have evidence.

**Component map:** `docs/audits/COMPLIANCE_COMPONENT_MAP.md`

### 3.1 Coverage (Component → Reviewed)
- [ ] Vault manager (`src/core/VaultManager*.cc`) reviewed
- [ ] Crypto (`src/core/crypto/`, `src/core/services/VaultCryptoService.cc`) reviewed
- [ ] File I/O + backups (`src/core/io/`, `src/core/services/VaultFileService.cc`) reviewed
- [ ] Formats/serialization (`src/core/format/`, `src/core/serialization/`) reviewed
- [ ] YubiKey (`src/core/managers/`, `src/core/services/VaultYubiKeyService.cc`, UI handlers) reviewed
- [ ] UI auth + clipboard + autolock reviewed
- [ ] Settings + validation reviewed
- [ ] Import/export reviewed
- [ ] Logging reviewed (redaction / no-secret guarantee)

For each component, list any findings (IDs):
- Vault manager findings:
- Crypto findings:
- File I/O findings:

### 3.2 CONTRIBUTING.md (Coding + Testing Standards)

#### Style / Structure
- [ ] Naming conventions followed (PascalCase classes, `snake_case` functions/vars, `m_` members)
	- Evidence:
- [ ] Formatting is consistent (4 spaces, 100 col guideline, braces)
	- Evidence:
- [ ] Include ordering respected (self header, C, C++, external, project)
	- Evidence:

#### Modern C++ / Safety
- [ ] RAII used for resources (files, memory, locks, crypto contexts)
	- Evidence:
- [ ] Ownership uses smart pointers; no unsafe ownership via raw `new/delete`
	- Evidence:
- [ ] `std::span` used for non-owning buffer views where appropriate (avoid pointer+size)
	- Evidence:
- [ ] Error handling is explicit and consistent; consider/justify `std::expected` where bool would hide causes
	- Evidence:

#### SRP / SOLID (Policy expectation)
- [ ] Critical modules have a single reason to change; orchestration vs crypto vs I/O separated (or a remediation ticket exists)
	- Evidence (notes or ticket IDs):

#### Tests (Required for changes)
- [ ] Relevant unit/integration tests exist for each audited security feature area
	- Evidence (list tests executed):
- [ ] New/updated tests cover both success and failure cases for any audit-driven code changes
	- Evidence:

### 3.3 SECURITY.md (Security Features → Code Evidence)

#### Crypto (AES-256-GCM, PBKDF2, salt/IV sizes)
- [ ] AES-256-GCM is used for authenticated encryption; tag verification is enforced and failure is handled safely
	- Evidence (files/symbols + tests):
- [ ] PBKDF2 is PBKDF2-HMAC-SHA256 with >= 100,000 iterations (or configurable policy with safe defaults)
	- Evidence (files/symbols + tests):
- [ ] Salt is random and 32 bytes; IV is random and 12 bytes per encryption operation
	- Evidence (files/symbols + tests):

#### Memory Protection (cleanse + lock)
- [ ] Sensitive buffers cleared with `OPENSSL_cleanse()` (not `memset`, not `.clear()` alone)
	- Evidence (files/symbols + tests):
- [ ] `mlock()` is used for sensitive buffers where intended; `munlock()` paths exist; failures are handled safely
	- Evidence (files/symbols + tests):

#### Clipboard Protection
- [ ] Clipboard auto-clears after ~30 seconds (or configured timeout with safe default)
	- Evidence (files/symbols + tests):
- [ ] Clipboard “preservation” mode (if present) cannot accidentally persist secrets long-term
	- Evidence:

#### File Security
- [ ] Atomic writes are used for vault saves; corruption risk on crash/interrupt is minimized
	- Evidence (files/symbols + tests):
- [ ] Automatic backups are created and restore paths are robust
	- Evidence:
- [ ] Vault and exported sensitive files are created with 0600 permissions (owner-only)
	- Evidence (files/symbols):

#### Input Validation / Password Policy
- [ ] Inputs have explicit length limits and bounds checks (UI + parsing + file formats)
	- Evidence (files/symbols + tests):
- [ ] Password policy aligns with stated goals (NIST SP 800-63B, common password prevention)
	- Evidence (tests):

### 3.4 FIPS-140-3 Ready Mode (CONTRIBUTING.md + FIPS docs)

#### Provider / API usage
- [ ] OpenSSL 3.x provider-based EVP APIs are used on FIPS paths (no deprecated low-level primitives)
	- Evidence (files/symbols):
- [ ] FIPS enablement is explicit (not implicit), and the app can detect “FIPS provider available vs not”
	- Evidence (files/symbols + tests):

#### Approved algorithms only (when FIPS enabled)
- [ ] AES-256-GCM only for encryption
	- Evidence:
- [ ] PBKDF2-HMAC-SHA256/SHA512 only for derivation (no MD5/SHA1)
	- Evidence:
- [ ] RNG/DRBG is provider-backed (no ad-hoc RNG)
	- Evidence:
- [ ] YubiKey integration uses approved algorithms when FIPS is enforced (e.g., HMAC-SHA256)
	- Evidence:

#### Behavioral expectations
- [ ] FIPS enabled + provider available → operations succeed using approved algorithms
	- Evidence (tests):
- [ ] FIPS requested but provider unavailable → app refuses “FIPS enabled” or clearly indicates it cannot enforce
	- Evidence (tests/logs):

### 3.5 Prior Review Regression Check (docs/audits/CODE_REVIEW.md)
- [ ] Secure erasure gaps are not regressed (keys/passwords cleared with `OPENSSL_cleanse`)
	- Evidence:
- [ ] Atomic write + error handling expectations are met
	- Evidence:
- [ ] Clipboard persistence risk is mitigated
	- Evidence:

---

## 4. Findings Summary

### Critical
-

### High
-

### Medium
-

### Low
-

---

## 5. Detailed Findings

(Reference individual findings from `docs/audits/COMPLIANCE_FINDINGS_TEMPLATE.md` or inline them here.)

---

## 6. Evidence: Commands and Outputs

### Builds
- `meson setup build`
- `meson compile -C build`

### Tests
- Gate B recommended set (copy outputs into this report):
	- `meson test -C build --print-errorlogs "FIPS Mode Tests"`
	- `meson test -C build --print-errorlogs "Memory Locking Security Tests"`
	- `meson test -C build --print-errorlogs vault_crypto "VaultCryptoService Unit Tests"`
	- `meson test -C build --print-errorlogs vault_io "VaultFileService Unit Tests"`
	- `meson test -C build --print-errorlogs "UI Security Tests" clipboard_manager_test auto_lock_manager_test`
	- `meson test -C build --print-errorlogs --suite migration`
	- Optional (broad): `meson test -C build --print-errorlogs`

### Sanitizers
- ASan/UBSan (example; use a dedicated builddir):
	- `meson setup build-asan -Db_sanitize=address,undefined -Db_lundef=false`
	- `meson compile -C build-asan`
	- Repeat Gate B tests against `build-asan` (same `meson test -C ...` commands)
- LeakSanitizer (if supported by toolchain; often via ASan):
	- Evidence:

### Static Analysis
- `clang-tidy` (example; adjust checks to project policy):
	- Evidence:

---

## 7. Remediation Backlog

- Ticket list with owners + priority + target release.

---

## 8. CI Gate Recommendations

- What should be blocking vs informational.
- Any suppressions or baselines and why.
