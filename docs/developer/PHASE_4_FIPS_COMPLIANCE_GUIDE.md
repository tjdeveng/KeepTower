# Phase 4 FIPS Compliance Implementation Guide

**Purpose**: Ensure users clearly understand FIPS-140-3 compliance implications when selecting cryptographic algorithms.

---

## Key Requirements

1. **Argon2id creates NON-FIPS-compliant vaults** - This must be VERY clear to users
2. **When FIPS mode enabled, Argon2id must be unselectable** - Not just warned, but prevented
3. **Consistent with existing username hashing FIPS enforcement** - Follow established pattern

---

## UI Messaging Strategy

### Algorithm Info Labels

#### SHA3 Variants (FIPS-Compliant)
```
ℹ️  Username: SHA3-256 (fast, FIPS-approved)
    Password KEK: PBKDF2-SHA256 (600K iterations)
    Passwords automatically protected with stronger algorithm.
```
**Key points**:
- Emphasize "FIPS-approved"
- Explain automatic upgrade is for security

#### PBKDF2 (FIPS-Compliant)
```
ℹ️  Username: PBKDF2-SHA256 (configurable iterations)
    Password KEK: PBKDF2-SHA256 (same parameters)
    Consistent security for both username and password. FIPS-approved.
```
**Key points**:
- Emphasize "FIPS-approved"
- Highlight consistency

#### Argon2id (NON-FIPS-Compliant) ⚠️
```
⚠️  Non-FIPS Vault:
    Username: Argon2id (memory-hard, configurable)
    Password KEK: Argon2id (same parameters)
    ⚠️  Vaults created with this algorithm are NOT FIPS-140-3 compliant.
    Maximum security but slower unlock (2-8 seconds).
```
**Key points**:
- Use orange warning color (`foreground='#f57900'`)
- **Prominently state "NON-FIPS Vault" at the start**
- **Bold warning about FIPS non-compliance**
- Still mention advantages (maximum security)

#### When FIPS Mode Enabled + Argon2id Selected
```
⚠️  FIPS MODE ACTIVE:
    This algorithm is not FIPS-approved and cannot be used.
    Please select a FIPS-approved algorithm (SHA3-256/384/512 or PBKDF2).
```
**Key points**:
- Use red error color (`foreground='#e01b24'`)
- Use word "cannot" not "will be blocked"
- List acceptable alternatives

---

## FIPS Mode Enforcement Implementation

### Pattern 1: Auto-Revert in Signal Handler (Recommended)

**File**: `src/ui/dialogs/PreferencesDialog.cc`

```cpp
void PreferencesDialog::on_username_hash_changed() noexcept {
    update_username_hash_info();
    update_username_hash_advanced_params();

    // FIPS mode enforcement - auto-revert to SHA3-256
    if (m_settings) {
        const bool fips_enabled = m_settings->get_boolean("fips-mode-enabled");
        const auto algorithm = m_username_hash_combo.get_active_id();

        if (fips_enabled && (algorithm == "plaintext" || algorithm == "argon2id")) {
            KeepTower::Log::warning("FIPS mode active: Cannot select {} algorithm, "
                                   "reverting to SHA3-256", algorithm);
            m_username_hash_combo.set_active_id("sha3-256");
            return;  // Will trigger handler again with valid selection
        }
    }
}
```

**Why this works**:
- Immediate feedback - user sees selection change instantly
- Prevents invalid state from persisting
- Consistent with GTK signal-based UI patterns

### Pattern 2: Override on Load

**File**: `src/ui/dialogs/PreferencesDialog.cc`

```cpp
void PreferencesDialog::load_settings() {
    // ... existing code ...

    // Load FIPS mode first
    bool fips_enabled = false;
    if (m_settings) {
        try {
            fips_enabled = m_settings->get_boolean("fips-mode-enabled");
        } catch (const Glib::Error& e) {
            KeepTower::Log::warning("Failed to read fips-mode-enabled: {}", e.what());
        }
    }

    // Load username hashing algorithm
    Glib::ustring username_hash_algorithm = m_settings->get_string("username-hash-algorithm");

    // If FIPS mode enabled and saved algorithm is non-FIPS, override
    if (fips_enabled && (username_hash_algorithm == "plaintext" ||
                         username_hash_algorithm == "argon2id")) {
        KeepTower::Log::info("FIPS mode enabled: Overriding saved algorithm {} with SHA3-256",
                            username_hash_algorithm);
        username_hash_algorithm = "sha3-256";
    }

    m_username_hash_combo.set_active_id(username_hash_algorithm);

    // ... rest of code ...
}
```

**Why this works**:
- Handles case where user enabled FIPS mode, restarted app
- Previously saved Argon2id selection is overridden
- Defense-in-depth

### Pattern 3: Conditional Dropdown Population (Optional)

**File**: `src/ui/dialogs/PreferencesDialog.cc`

```cpp
void PreferencesDialog::setup_vault_security_page() {
    // ... existing code ...

    // Populate dropdown with available algorithms
    m_username_hash_combo.append("plaintext", "Plaintext (DEPRECATED)");
    m_username_hash_combo.append("sha3-256", "SHA3-256 (FIPS)");
    m_username_hash_combo.append("sha3-384", "SHA3-384 (FIPS)");
    m_username_hash_combo.append("sha3-512", "SHA3-512 (FIPS)");
    m_username_hash_combo.append("pbkdf2-sha256", "PBKDF2-SHA256 (FIPS)");

#ifdef ENABLE_ARGON2
    // Only add Argon2id if FIPS mode NOT currently enabled
    bool fips_enabled = false;
    if (m_settings) {
        try {
            fips_enabled = m_settings->get_boolean("fips-mode-enabled");
        } catch (const Glib::Error& e) {
            // Default to allowing Argon2id if can't read FIPS setting
        }
    }

    if (!fips_enabled) {
        m_username_hash_combo.append("argon2id", "Argon2id (non-FIPS)");
    }
#endif

    // ... rest of code ...
}
```

**Pros**:
- Argon2id not even visible when FIPS mode enabled
- Most obvious to user

**Cons**:
- Requires rebuilding dropdown if FIPS mode toggled at runtime
- More complex

**Recommendation**: Use Patterns 1 + 2 (auto-revert + override on load) for simplicity and robustness.

---

## Current Vault Display

When showing current vault's KEK algorithm, also indicate FIPS compliance:

```cpp
std::string kek_algo_display;

if (policy.username_hash_algorithm >= 0x01 && policy.username_hash_algorithm <= 0x03) {
    // SHA3 → KEK uses PBKDF2 (FIPS-compliant)
    kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
} else if (policy.username_hash_algorithm == 0x04) {
    // PBKDF2 (FIPS-compliant)
    kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
} else if (policy.username_hash_algorithm == 0x05) {
    // Argon2id (NON-FIPS)
    kek_algo_display = "Argon2id (⚠️ non-FIPS vault)";
} else {
    kek_algo_display = "PBKDF2-HMAC-SHA256 (default, FIPS)";
}

m_current_kek_label.set_markup(
    "<span>Password KEK Algorithm: <b>" + kek_algo_display + "</b></span>");
```

**Key points**:
- FIPS-compliant algorithms show "(FIPS)" suffix
- Argon2id shows "(⚠️ non-FIPS vault)" to reinforce this is a vault property
- User can see at a glance if their open vault is FIPS-compliant

---

## Help Documentation

The help page should include a FIPS compliance section:

### Example Section

```markdown
## FIPS-140-3 Compliance

### What is FIPS?

FIPS-140-3 is a U.S. government security standard for cryptographic modules.
Organizations in regulated industries (government, healthcare, finance) may
require FIPS-approved cryptography.

### Which Algorithms are FIPS-Approved?

**✅ FIPS-Approved (creates FIPS-compliant vaults):**
- SHA3-256, SHA3-384, SHA3-512 (for usernames)
- PBKDF2-HMAC-SHA256 (for master password KEK)

**❌ NOT FIPS-Approved (creates NON-FIPS-compliant vaults):**
- Argon2id (for both usernames and passwords)

### Important: Vault-Level Property

FIPS compliance is determined **at vault creation** by the algorithm selected.
Once a vault is created:
- A vault created with SHA3/PBKDF2 is **always FIPS-compliant**
- A vault created with Argon2id is **never FIPS-compliant**

You cannot upgrade or downgrade a vault's FIPS compliance status.

### FIPS Mode Enforcement

KeepTower's optional "FIPS Mode" (Preferences → Vault Security) enforces
FIPS compliance for **all new vaults**:

- **When FIPS Mode Enabled**: Argon2id cannot be selected
- **When FIPS Mode Disabled**: All algorithms available (default)

**Note**: FIPS Mode is a user preference for creating new vaults. It does
not affect your ability to open existing Argon2id vaults.

### Should I Enable FIPS Mode?

**Enable if:**
- You work in regulated industries (government, defense, healthcare)
- Your organization's security policy requires FIPS-140-3
- You need to store vaults on government systems

**Disable if:**
- You want maximum cryptographic security (Argon2id)
- You're an individual user without regulatory requirements
- You prioritize GPU/ASIC resistance over certification

### Can I Open Argon2id Vaults in FIPS Mode?

**Yes!** FIPS Mode only restricts **creating** new vaults. You can always
open existing vaults regardless of their algorithm.

Example:
1. Create vault with Argon2id (FIPS Mode disabled)
2. Enable FIPS Mode
3. Can still open and use Argon2id vault
4. Cannot create NEW Argon2id vaults (will use SHA3-256 instead)

This allows transitioning to FIPS compliance without losing access to
existing data.
```

---

## Testing Checklist

### FIPS Mode Enforcement
- [ ] Enable FIPS mode, verify Argon2id not selectable
- [ ] Select Argon2id, enable FIPS mode, verify auto-reverts to SHA3-256
- [ ] Save Argon2id selection, enable FIPS mode, restart app, verify loads SHA3-256
- [ ] Create vault in FIPS mode, verify uses PBKDF2 for KEK
- [ ] Disable FIPS mode, verify Argon2id selectable again

### UI Messaging
- [ ] Select Argon2id, verify shows "NON-FIPS vault" warning prominently
- [ ] Select Argon2id with FIPS mode enabled, verify shows "cannot be used" error
- [ ] Select SHA3-256, verify shows "FIPS-approved" label
- [ ] Select PBKDF2, verify shows "FIPS-approved" label
- [ ] Open Argon2id vault, verify Current Vault shows "(⚠️ non-FIPS vault)"
- [ ] Open SHA3 vault, verify Current Vault shows "(FIPS)" suffix

### Backend Integration
- [ ] Try to create Argon2id vault when FIPS mode enabled (should fail at backend)
- [ ] Verify SettingsValidator enforces FIPS restrictions
- [ ] Verify VaultCreationOrchestrator respects FIPS mode

---

## Implementation Priority

**High Priority** (Must Have):
1. ✅ Argon2id shows "NON-FIPS vault" warning prominently
2. ✅ FIPS mode prevents Argon2id selection (auto-revert)
3. ✅ Current vault display shows FIPS compliance status

**Medium Priority** (Should Have):
4. ✅ Help documentation explains FIPS compliance
5. ✅ Override saved Argon2id selection on load if FIPS enabled

**Low Priority** (Nice to Have):
6. ⭕ Dynamic dropdown rebuild when FIPS mode toggled
7. ⭕ Visual indicator (icon) for FIPS-compliant vaults in vault list

---

## User Experience Considerations

### Avoid Confusion

**❌ Don't say**: "Argon2id is not FIPS-compliant"
**✅ Do say**: "Vaults created with Argon2id are not FIPS-compliant"

**Reason**: The algorithm itself is cryptographically sound. FIPS certification
is about standards compliance, not security strength. Frame it as a vault
property, not an algorithm flaw.

### Positive Framing

**❌ Don't say**: "Argon2id is blocked in FIPS mode"
**✅ Do say**: "FIPS mode ensures all new vaults meet FIPS-140-3 standards"

**Reason**: Focus on what FIPS mode enables (compliance), not what it restricts.

### Educational Tone

Always explain **why** FIPS compliance matters:
- Regulatory requirements
- Government/healthcare/finance industry needs
- Certification for enterprise deployment

And explain **why** some users don't need it:
- Individual users without regulatory requirements
- Maximum security often means non-FIPS algorithms (Argon2id > PBKDF2)

---

## Code Review Checklist

When reviewing Phase 4 implementation, verify:

- [ ] Argon2id warning includes "NON-FIPS vault" prominently
- [ ] FIPS mode enforcement implemented in signal handler
- [ ] FIPS mode enforcement implemented in load_settings()
- [ ] Current vault display shows FIPS status
- [ ] Help documentation explains FIPS compliance thoroughly
- [ ] User cannot create Argon2id vault when FIPS mode enabled
- [ ] User can still open Argon2id vaults when FIPS mode enabled
- [ ] Logging added for FIPS enforcement actions
- [ ] No hardcoded assumptions (use m_settings->get_boolean("fips-mode-enabled"))

---

**Document Status**: ✅ Ready for Implementation
**Next Steps**: Review this guide before implementing Task 1 FIPS enforcement
