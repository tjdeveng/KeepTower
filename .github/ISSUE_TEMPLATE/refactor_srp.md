---
name: Refactor / SRP (Maintainability)
about: Track SRP/SOLID refactors and technical-debt reductions (low-churn, test-driven)
title: '[REFACTOR] '
labels: maintainability, srp, security, audit
assignees: ''
---

## Summary
A clear, concise description of what will be refactored and why.

## Motivation / SRP Violation
What single responsibility is currently violated (or what responsibilities are coupled)?

- **Current hotspot(s):**
  - [e.g., `src/core/VaultManager.cc`]
- **Symptoms:**
  - [ ] Too many responsibilities in one class/module
  - [ ] Complex/long functions in security-critical paths
  - [ ] High include/build coupling
  - [ ] UI ↔ core layering violations
  - [ ] Hard to test (needs UI/IO to test core behavior)

## Proposed Change (Minimal, Low-Churn)
Describe the smallest safe change that improves boundaries.

- **New types/modules (if any):**
  - [e.g., `FipsProviderManager`]
- **Public API impact:**
  - [ ] None (preferred)
  - [ ] Minor (describe)
  - [ ] Breaking (avoid unless explicitly approved)

## Acceptance Criteria
- [ ] Behavior unchanged (except where explicitly stated)
- [ ] Tests added/updated for new boundaries
- [ ] No new sensitive logging introduced
- [ ] Passes Gate B security test set (`meson test -C build --print-errorlogs ...`)
- [ ] Passes sanitizer subset when applicable (ASan/UBSan)

## Risk & Rollout
- **Risk level:** Low / Medium / High
- **Mitigations:**
  - [e.g., do refactor behind internal helper with no header changes]

## Test Plan
Commands and/or test names.

- `meson test -C build --print-errorlogs <tests>`
- `meson test -C build-asan --print-errorlogs <tests>` (if applicable)

## Notes / References
- Related audit notes:
  - `docs/audits/SRP_SOLID_HOTSPOTS_2026-02-02.md`
- Links to related issues/PRs:
  - #
