# Phase 4 Implementation Checklist

**Use this checklist to track progress during implementation**

## Pre-Implementation

- [ ] Read full plan: `KEK_DERIVATION_PHASE_4_PLAN.md`
- [ ] Review `CONTRIBUTING.md` SRP section
- [ ] Review existing username hash UI code in `PreferencesDialog.cc`
- [ ] Review existing vault password history display pattern
- [ ] Set up test vault with each algorithm type (SHA3, PBKDF2, Argon2id)

---

## Task 1: Update Preferences UI Copy (2-3 hours)

### Section Titles
- [ ] Change "Username Hashing Algorithm" → "Key Derivation Algorithm"
- [ ] Keep "(New Vaults Only)" subtitle

### Section Description
- [ ] Update description to mention "usernames and master passwords"

### Info Label Updates (`update_username_hash_info()`)
- [ ] SHA3-256: Show "Username: SHA3-256" and "Password KEK: PBKDF2"
- [ ] SHA3-384: Show "Username: SHA3-384" and "Password KEK: PBKDF2"
- [ ] SHA3-512: Show "Username: SHA3-512" and "Password KEK: PBKDF2"
- [ ] PBKDF2: Show both use "PBKDF2-HMAC-SHA256 (same parameters)" with "FIPS-approved" note
- [ ] Argon2id: Show both use "Argon2id (same parameters)" with **"⚠️ NON-FIPS vault"** warning
- [ ] Add "Passwords automatically protected with stronger algorithm" for SHA3
- [ ] Add "Unlock may take 2-8 seconds" warning for Argon2id
- [ ] **Add prominent "Vaults created with this algorithm are NOT FIPS-140-3 compliant" for Argon2id**
- [ ] Keep existing FIPS mode warning logic

### FIPS Mode Enforcement (NEW)
- [ ] Add FIPS enforcement check to `on_username_hash_changed()`
- [ ] If FIPS mode enabled and Argon2id selected, revert to SHA3-256
- [ ] Add FIPS enforcement check to `load_settings()`
- [ ] If FIPS mode enabled and saved algorithm is Argon2id, override with SHA3-256
- [ ] Consider: Don't add Argon2id to dropdown if FIPS mode enabled at startup
- [ ] Consider: Add signal handler for FIPS mode toggle to rebuild dropdown

### Manual Testing Task 1
- [ ] Select SHA3-256, verify info shows PBKDF2 upgrade and FIPS-approved
- [ ] Select SHA3-384, verify info shows PBKDF2 upgrade and FIPS-approved
- [ ] Select SHA3-512, verify info shows PBKDF2 upgrade and FIPS-approved
- [ ] Select PBKDF2, verify info shows consistency and FIPS-approved
- [ ] Select Argon2id, verify info shows **"NON-FIPS vault"** warning prominently
- [ ] Select Argon2id, verify info shows performance warning
- [ ] Enable FIPS mode, verify Argon2id automatically reverts to SHA3-256
- [ ] Enable FIPS mode, verify Argon2id shows "cannot be used" warning
- [ ] Try to manually select Argon2id when FIPS enabled, verify reverts to SHA3-256
- [ ] Disable FIPS mode, verify Argon2id can be selected again

---

## Task 2: Display Current Vault KEK (4-5 hours)

### Header File (`PreferencesDialog.h`)
- [ ] Add `Gtk::Box* m_current_vault_kek_box` member
- [ ] Add `Gtk::Label m_current_username_hash_label` member
- [ ] Add `Gtk::Label m_current_kek_label` member
- [ ] Add `Gtk::Label m_current_kek_params_label` member
- [ ] Add `void update_current_vault_kek_info() noexcept;` declaration

### Implementation (`PreferencesDialog.cc`)
- [ ] Create "Current Vault Security" section in `setup_vault_security_page()`
- [ ] Add section title label
- [ ] Add section description label
- [ ] Add `m_current_username_hash_label` to box
- [ ] Add `m_current_kek_label` to box
- [ ] Add `m_current_kek_params_label` to box
- [ ] Set box initially invisible
- [ ] Append box to `m_vault_security_box`

### `update_current_vault_kek_info()` Implementation
- [ ] Check if vault is open, hide box if not
- [ ] Get `VaultSecurityPolicy` from VaultManager
- [ ] Map `username_hash_algorithm` to display string (0x00-0x05)
- [ ] Infer KEK algorithm (SHA3→PBKDF2, else same as username)
- [ ] Display KEK parameters (iterations for PBKDF2, memory/time for Argon2id)
- [ ] Set label text with proper formatting
- [ ] Handle error cases (vault not open, policy unavailable)

### Integration
- [ ] Call `update_current_vault_kek_info()` from `on_dialog_shown()`

### Manual Testing Task 2
- [ ] Create vault with SHA3-256, open Preferences, verify shows "KEK: PBKDF2"
- [ ] Create vault with PBKDF2, open Preferences, verify shows "KEK: PBKDF2"
- [ ] Create vault with Argon2id, open Preferences, verify shows "KEK: Argon2id"
- [ ] Close vault, reopen Preferences, verify section hidden
- [ ] Open different vault, verify section updates

---

## Task 3: Help Documentation (6-8 hours)

### Create Help Page (`docs/help/user/05-vault-security-algorithms.md`)
- [ ] Introduction paragraph (2 ways KeepTower protects vaults)
- [ ] "Algorithm Selection" section
- [ ] SHA3 variants explanation (with auto-upgrade)
- [ ] PBKDF2 explanation
- [ ] Argon2id explanation
- [ ] Algorithm comparison table
- [ ] Security recommendations by use case
- [ ] FAQ section (5-7 common questions)
- [ ] Technical details section (attack scenarios)
- [ ] "See Also" links

### Update Help System
- [ ] Add entry to `docs/help/user/README.md` index
- [ ] Add `VaultSecurityAlgorithms` to `HelpTopic` enum in `HelpManager.h`
- [ ] Add topic mapping in `HelpManager.cc` (return `"user/05-vault-security-algorithms.html"`)

### Add Help Button to Preferences
- [ ] Create help button with "help-about-symbolic" icon
- [ ] Set tooltip "Learn about KEK derivation algorithms"
- [ ] Connect signal to open help via `HelpManager::open_help()`
- [ ] Add button to username_hash_row in Preferences

### Manual Testing Task 3
- [ ] Click help button, verify opens in browser
- [ ] Verify all sections render correctly
- [ ] Verify links work
- [ ] Verify table formatting correct
- [ ] Read through for clarity and accuracy

---

## Task 4: Advanced Parameters Help (30 min)

### Update Help Text
- [ ] Find `m_username_hash_advanced_help.set_markup()` call
- [ ] Change text to mention "both username hashing and password KEK derivation"
- [ ] Keep existing warning about higher values

### Manual Testing Task 4
- [ ] Select PBKDF2, expand Advanced Parameters, verify help text
- [ ] Select Argon2id, expand Advanced Parameters, verify help text

---

## Task 5: Performance Warnings (2-3 hours)

### Header File
- [ ] Add `double estimate_argon2_time(uint32_t memory_kb, uint32_t time_cost) const noexcept;`

### Implementation
- [ ] Implement `estimate_argon2_time()` with formula: `(mem/65536) * time * 0.5`
- [ ] Update `update_username_hash_advanced_params()` to show warnings
- [ ] Call `estimate_argon2_time()` when Argon2id selected
- [ ] Show orange warning if estimated time >3 seconds
- [ ] Show normal info if estimated time ≤3 seconds
- [ ] Include estimated time in message

### Connect Signals (Optional but Nice)
- [ ] Connect `m_argon2_memory_spin.signal_value_changed()` to update warning
- [ ] Connect `m_argon2_time_spin.signal_value_changed()` to update warning

### Manual Testing Task 5
- [ ] Select Argon2id, set 64MB + 3 time, verify ~0.5s estimate
- [ ] Set 128MB + 3 time, verify ~1s estimate
- [ ] Set 256MB + 5 time, verify ~2-3s estimate, orange warning
- [ ] Set 512MB + 10 time, verify ~8s estimate, orange warning
- [ ] Adjust sliders, verify live updates (if signals connected)

---

## Task 6: Full Integration Testing (4-6 hours)

### SHA3 Algorithm Flow
- [ ] Open Preferences, select SHA3-256
- [ ] Verify info shows "Username: SHA3-256, Password KEK: PBKDF2"
- [ ] Create vault with SHA3-256
- [ ] Unlock vault, measure time (~1 second)
- [ ] Open Preferences, verify "Current Vault" shows KEK=PBKDF2
- [ ] Close Preferences, close vault

### PBKDF2 Algorithm Flow
- [ ] Open Preferences, select PBKDF2
- [ ] Set iterations to 600,000
- [ ] Verify info shows both use PBKDF2
- [ ] Create vault with PBKDF2
- [ ] Unlock vault, measure time (~1 second)
- [ ] Open Preferences, verify "Current Vault" shows KEK=PBKDF2, 600K iterations
- [ ] Increase iterations to 1M, create new vault
- [ ] Unlock new vault, measure time (~1.7 seconds)

### Argon2id Algorithm Flow
- [ ] Open Preferences, select Argon2id
- [ ] Set 64MB memory, 3 time cost
- [ ] Verify info shows performance estimate
- [ ] Create vault with Argon2id
- [ ] Unlock vault, measure time (compare to estimate)
- [ ] Open Preferences, verify "Current Vault" shows KEK=Argon2id, params
- [ ] Set 256MB memory, 5 time cost
- [ ] Verify orange warning shows
- [ ] Create new vault
- [ ] Unlock, measure time (should be ~2-3 seconds)

### FIPS Mode Testing
- [ ] Enable FIPS mode in Preferences
- [ ] Verify Argon2id is NOT selectable (either disabled or auto-reverts to SHA3-256)
- [ ] Verify Argon2id shows "cannot be used" warning if somehow selected
- [ ] Try to create vault - verify backend also blocks Argon2id (defense-in-depth)
- [ ] Select SHA3-256, create vault successfully
- [ ] Verify vault is FIPS-compliant (uses PBKDF2 for KEK)
- [ ] Disable FIPS mode
- [ ] Verify Argon2id becomes selectable again

### Current Vault Display Testing
- [ ] Open vault (any algorithm)
- [ ] Open Preferences
- [ ] Verify "Current Vault Security" visible
- [ ] Close vault (without closing Preferences)
- [ ] Close and reopen Preferences
- [ ] Verify "Current Vault Security" hidden
- [ ] Open different vault (different algorithm)
- [ ] Open Preferences
- [ ] Verify shows correct algorithm for new vault

### Help System Testing
- [ ] Click help button in Preferences
- [ ] Verify help page opens
- [ ] Read through entire help page for accuracy
- [ ] Verify attack scenario examples make sense
- [ ] Verify algorithm comparison table accurate
- [ ] Click "See Also" links, verify they work

### Edge Cases
- [ ] Open Preferences before any vault created
- [ ] Verify sensible defaults shown
- [ ] Create vault, delete vault, open Preferences
- [ ] Verify handles missing vault gracefully

---

## Code Quality Checks

### SRP Compliance
- [ ] `PreferencesDialog` only does UI (no crypto, no vault logic)
- [ ] All vault info comes from `VaultManager` (no direct file access)
- [ ] Help system uses `HelpManager` (no direct file opening)
- [ ] No business logic in UI event handlers

### CONTRIBUTING.md Compliance
- [ ] All string formatting uses Glib::ustring properly
- [ ] No memory leaks (use Gtk::make_managed<> for widgets)
- [ ] Error handling with try/catch where appropriate
- [ ] Const correctness on methods
- [ ] Noexcept on UI update methods

### Documentation
- [ ] All new methods have Doxygen comments
- [ ] Member variables have brief descriptions
- [ ] Help page has clear examples

---

## Final Checklist

- [ ] All tasks 1-5 complete
- [ ] All manual tests passed
- [ ] Code compiles without warnings
- [ ] No segfaults or crashes
- [ ] Help button works
- [ ] Current vault display works
- [ ] Performance warnings accurate
- [ ] FIPS mode enforcement working
- [ ] Git commit with clear message
- [ ] Push to remote

---

## Commit Message Template

```
feat: Add KEK derivation UI and help documentation (Phase 4)

Complete Phase 4 of KEK derivation security enhancement:
- Update Preferences UI to show KEK algorithm for usernames AND passwords
- Add current vault KEK algorithm display (read-only)
- Add comprehensive help documentation explaining algorithms
- Add performance warnings for Argon2id high-memory settings
- Clarify SHA3→PBKDF2 automatic upgrade for password security

Changes:
- Update algorithm section title and description
- Enhance update_username_hash_info() to show Username/Password KEK split
- Add update_current_vault_kek_info() method and UI components
- Create docs/help/user/05-vault-security-algorithms.md
- Add HelpTopic::VaultSecurityAlgorithms enum value
- Implement estimate_argon2_time() for performance warnings

Testing:
- Manual testing with SHA3, PBKDF2, Argon2id algorithms
- Vault open/closed state transitions
- FIPS mode enforcement
- Help system integration

Follows SRP (UI only, no backend changes).
Refs: KEK_DERIVATION_PHASE_4_PLAN.md
```

---

**Progress Tracking**: Check off each item as you complete it.
**Estimated Time**: 20-25 hours total (3-4 days)
**Blockers**: Document here if you encounter any issues
