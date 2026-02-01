# Compliance Audit Findings Template

**Project:** KeepTower

**Audit name:** CONTRIBUTING.md + SECURITY.md Compliance Audit

**Date:** YYYY-MM-DD

**Auditor(s):**

**Scope reviewed:** (e.g., crypto, vault I/O, UI secrets, tests)

---

## How to Use This Template

- Copy one **Finding** section per issue discovered.
- Keep findings evidence-based: include file paths + symbols + repro steps when possible.
- Prefer minimal, security-driven fixes (avoid refactor churn).

---

## Severity Definitions

- **Critical:** likely compromise of secrets, crypto misuse in FIPS mode, persistent plaintext exposure, or remotely triggerable memory corruption.
- **High:** serious weakness that could expose secrets or cause corruption with realistic conditions.
- **Medium:** defense-in-depth gaps, hardening issues, or moderate maintainability risks in critical code.
- **Low:** minor style/maintainability issues, non-exploitable warnings, documentation gaps.

---

## Finding: <Short Title>

- **ID:** CA-YYYY-MM-DD-###
- **Severity:** Critical | High | Medium | Low
- **Category:** SRP | FIPS | memory | crypto | I/O | logging | UI | build | tests | docs
- **Status:** Open | In Progress | Fixed | Won’t Fix (justify)
- **Owner:** @

### Summary
(1–3 sentences describing the issue.)

### Policy/Requirement
- Which policy this relates to (e.g., root CONTRIBUTING.md “Memory Safety”, SECURITY.md “Atomic Writes”, FIPS_COMPLIANCE.md “Approved algorithms only”).

### Evidence
- **File(s):**
- **Symbol(s):**
- **Notes:**

### Impact
- What can go wrong and who it affects.
- For security issues: describe realistic attacker model/conditions.

### Reproduction / Validation Steps
- Steps to reproduce (if applicable).
- How to confirm the fix (test/sanitizer/static-analysis command).

### Recommended Fix (Minimal)
- Concrete fix suggestion.
- If multiple options exist, list “Preferred” and “Alternative”.

### Fix Verification Checklist
- [ ] Unit/integration test added or updated (if applicable)
- [ ] `meson test -C build` passes
- [ ] Sanitizer run clean for relevant tests (ASan/UBSan/LSan if available)
- [ ] No sensitive data added to logs
- [ ] Documentation updated (if behavior changed)

### Links
- Related issues/PRs:
- Related docs:

---

## Notes / Triage Decisions
- Suppressions used (if any):
- Risk accepted (if any):
- Follow-up tasks:
