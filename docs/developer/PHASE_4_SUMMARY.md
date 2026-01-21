# Phase 4 Implementation Summary

**Status**: Ready to start
**Date**: 2026-01-21
**Document**: [KEK_DERIVATION_PHASE_4_PLAN.md](KEK_DERIVATION_PHASE_4_PLAN.md)

## Quick Overview

Phase 4 completes the KEK derivation security enhancement by adding **user-facing UI components** to educate and inform users about the security algorithms protecting their vaults.

### What Was Completed (Phases 1-3)

✅ **Phase 1**: Core `KekDerivationService` (PBKDF2 and Argon2id support)
✅ **Phase 2**: `VaultFormatV2` extended with KEK algorithm fields
✅ **Phase 3**: `VaultManager` integration (all 44 tests passing)

**Backend is 100% complete and working!**

### What Phase 4 Adds (UI Only)

📊 **Information Display** - Show users what algorithms their vaults use
📚 **Education** - Help documentation explaining the security choices
⚠️ **Warnings** - Performance warnings for Argon2id high-memory settings
🔒 **Transparency** - Display current vault's KEK algorithm (read-only)

## Key Design Principles

1. **Information-only**: No backend changes, purely presentational
2. **SRP compliant**: PreferencesDialog displays info, doesn't change business logic
3. **Pattern reuse**: Follows existing vault password history display pattern
4. **Security-first**: Clear messaging about SHA3→PBKDF2 automatic upgrade

## Main Tasks

### Task 1: Update Preferences UI Copy (2-3 hours)
Update existing algorithm dropdown to clarify it applies to **both** username AND password:
- Change "Username Hashing" → "Key Derivation Algorithm"
- Split info labels: "Username: SHA3-256" / "Password KEK: PBKDF2-SHA256"
- Add messaging about automatic SHA3→PBKDF2 upgrade for passwords
- **Add prominent "NON-FIPS vault" warning for Argon2id selections**
- **Add FIPS mode enforcement: Make Argon2id unselectable when FIPS enabled**

### Task 2: Display Current Vault KEK (4-5 hours)
Show current vault's security settings (read-only, like password history):
- Add "Current Vault Security" section
- Display username hash algorithm
- Display password KEK algorithm
- Display parameters (iterations/memory)
- Show/hide based on vault open state

### Task 3: Help Documentation (6-8 hours)
Create comprehensive help page explaining:
- Why SHA3 is great for usernames, terrible for passwords
- Algorithm comparison table
- Security recommendations by use case
- Attack scenario examples (why KDFs are slow)
- FAQ section

### Task 4: Advanced Parameters Help (30 min)
Clarify that PBKDF2 iterations and Argon2 memory apply to **both** operations.

### Task 5: Performance Warnings (2-3 hours)
Warn users about Argon2id unlock times:
- Calculate estimated unlock time based on memory/time settings
- Show warning if >3 seconds
- Update dynamically as user adjusts parameters

### Task 6: Testing (4-6 hours)
Manual testing checklist covering all algorithm types and vault states.

## Total Effort

**~20-25 hours** (3-4 days for 1 developer)

## Critical Security Messaging

The key UI challenges are:

**1. Explain automatic SHA3→PBKDF2 upgrade clearly:**
```
User selects: SHA3-256
Result:
  - Username: SHA3-256 (fast)
  - Password: PBKDF2 (secure) ← AUTOMATIC UPGRADE

Why? SHA3 is too fast for passwords (millions of attempts/sec).
PBKDF2 slows attackers to ~1 attempt/sec.
```

**2. Make FIPS compliance crystal clear:**
```
User selects: Argon2id
Result:
  ⚠️ NON-FIPS VAULT
  - Username: Argon2id
  - Password: Argon2id

⚠️ Vaults created with this algorithm are NOT FIPS-140-3 compliant.

If FIPS mode enabled:
  → Argon2id cannot be selected (reverts to SHA3-256)
  → Show clear error: "This algorithm is not FIPS-approved and cannot be used"
```

All UI text emphasizes these are **good security practices**, not limitations.

## Success Criteria

✅ Users understand SHA3 selections automatically use PBKDF2 for passwords
✅ Users aware of Argon2id performance trade-offs (2-8 sec unlock)
✅ **Users clearly understand Argon2id creates NON-FIPS-compliant vaults**
✅ **Argon2id cannot be selected when FIPS mode is enabled**
✅ Current vault KEK algorithm displayed (read-only)
✅ Help documentation answers all common questions
✅ Code follows SRP (PreferencesDialog = UI only)
✅ No backend changes (Phase 1-3 code untouched)

## Files to Modify

**Existing Files**:
- `src/ui/dialogs/PreferencesDialog.cc` (update UI text, add current vault display)
- `src/ui/dialogs/PreferencesDialog.h` (add member variables for new labels)
- `src/utils/helpers/HelpManager.h` (add HelpTopic enum value)
- `src/utils/helpers/HelpManager.cc` (add topic mapping)

**New Files**:
- `docs/help/user/05-vault-security-algorithms.md` (comprehensive help page)

**No changes to**:
- ❌ `src/core/services/KekDerivationService.*` (Phase 1 complete)
- ❌ `src/core/VaultManagerV2.cc` (Phase 3 complete)
- ❌ `src/core/MultiUserTypesV2.h` (Phase 2 complete)

## Getting Started

1. Read the full plan: [KEK_DERIVATION_PHASE_4_PLAN.md](KEK_DERIVATION_PHASE_4_PLAN.md)
2. Review CONTRIBUTING.md for SRP guidelines
3. Start with Task 1 (simplest, validates approach)
4. Test each task before moving to next
5. Create help documentation last (once UI finalized)

## Questions Answered by the Plan

✅ What UI changes are needed?
✅ How do we display current vault info without violating SRP?
✅ How do we explain the SHA3→PBKDF2 upgrade clearly?
✅ What help documentation is needed?
✅ How do we warn about Argon2id performance?
✅ What testing is required?
✅ How does this fit with CONTRIBUTING.md principles?

---

**Next Action**: Review the detailed plan, then begin Task 1 (Update Preferences UI Copy).
