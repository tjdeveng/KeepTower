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

### CONTRIBUTING.md
- [ ] Style & formatting consistent
- [ ] RAII/smart pointers used for ownership
- [ ] `std::expected`/error handling patterns consistent
- [ ] SRP/SOLID adhered to in critical modules

### SECURITY.md
- [ ] Sensitive data cleared with `OPENSSL_cleanse()` (not only `.clear()`)
- [ ] Memory locking (`mlock`) used where appropriate
- [ ] Clipboard auto-clear behavior present
- [ ] Atomic writes + 0600 permissions enforced
- [ ] Input validation and length limits enforced

### FIPS-140-3 Ready Mode
- [ ] OpenSSL 3.x provider architecture used
- [ ] Only approved algorithms reachable when FIPS enabled
- [ ] FIPS enable/disable and fallback behavior is explicit and tested

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
- `meson setup build ...`
- `meson compile -C build`

### Tests
- `meson test -C build`
- `meson test -C build "FIPS Mode Tests"`

### Sanitizers
- ASan/UBSan commands:

### Static Analysis
- `clang-tidy` commands:

---

## 7. Remediation Backlog

- Ticket list with owners + priority + target release.

---

## 8. CI Gate Recommendations

- What should be blocking vs informational.
- Any suppressions or baselines and why.
