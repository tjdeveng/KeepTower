# KEK Derivation Algorithm Enhancement - Phase 4 Implementation Plan

**Version**: 1.0
**Date**: 2026-01-21
**Status**: Ready for Implementation
**Previous Phases**: ✅ Phase 1-3 Complete (Core service, VaultFormat, VaultManager integration)
**Author**: KeepTower Development Team

---

## Executive Summary

Phase 4 completes the KEK derivation enhancement by adding user-facing UI components to display and educate users about the KEK derivation algorithm. This phase builds upon the completed username hashing preferences UI and follows the same design patterns for consistency.

**Key Objectives:**
1. Update Preferences dialog to clarify that algorithm selection applies to both username AND master password KEK derivation
2. Display current vault's KEK algorithm in Preferences when vault is open (read-only)
3. Add educational help documentation about KEK algorithms
4. Ensure messaging clarifies automatic SHA3→PBKDF2 upgrade for password security
5. Maintain strict SRP compliance and follow CONTRIBUTING.md standards

**Design Philosophy:**
- **Information-focused**: Display, educate, don't change existing vaults
- **Security-first**: Clear warnings about algorithm implications
- **Consistency**: Reuse existing UI patterns from username hashing and vault password history
- **Read-only vault info**: Show current vault's algorithm, but don't allow changes (prevents security downgrades)

---

## Context from Previous Phases

### Completed Work (Phases 1-3)

✅ **Phase 1**: `KekDerivationService` - Pure crypto KEK derivation with PBKDF2 and Argon2id support
✅ **Phase 2**: `VaultFormatV2` extended with `kek_derivation_algorithm` field in `KeySlotV2`
✅ **Phase 3**: `VaultManager` integration - All vault operations use `KekDerivationService`

**Existing UI Infrastructure:**
- ✅ `PreferencesDialog::setup_vault_security_page()` has username hashing algorithm dropdown
- ✅ `update_username_hash_info()` pattern for dynamic info labels
- ✅ `update_vault_password_history_ui()` pattern for showing current vault info vs defaults
- ✅ GSettings integration for persisting preferences
- ✅ FIPS mode enforcement in UI

### Key Security Architecture Decision

**Intelligent Algorithm Mapping** (Already Implemented in Backend):
```cpp
// User selects SHA3 for usernames → Automatic PBKDF2 upgrade for passwords
Algorithm kek_algo = (pref_algo == SHA3_256 || pref_algo == SHA3_384 || pref_algo == SHA3_512)
    ? PBKDF2_HMAC_SHA256  // Automatic upgrade for security
    : pref_algo;           // Use user's choice (PBKDF2/Argon2id)
```

**UI Challenge**: Clearly communicate this automatic upgrade without confusing users.

---

## Design Principles (SRP & CONTRIBUTING.md Compliance)

### Single Responsibility Principle

| Class | Responsibility | Phase 4 Changes |
|-------|----------------|-----------------|
| `PreferencesDialog` | Display preferences UI, load/save settings | ✅ Update labels and info text only |
| `KekDerivationService` | Pure KEK derivation | ❌ No changes (Phase 1 complete) |
| `VaultManager` | Vault operations orchestration | ❌ No changes (Phase 3 complete) |
| `SettingsValidator` | Validate preferences | ❌ Already has validation methods |
| `HelpManager` | Help documentation access | ✅ Add KEK algorithm help page |

**SRP Compliance**: Phase 4 is purely presentational. No business logic changes.

### Security Considerations

1. **Read-only vault info**: Never allow downgrading KEK algorithm on existing vaults
2. **Clear warnings**: SHA3 selections automatically use PBKDF2 for passwords
3. **FIPS enforcement**:
   - Make Argon2id unselectable when FIPS mode is enabled (not just show warning)
   - Clear messaging that Argon2id creates non-FIPS-compliant vaults
   - Follow existing username hashing pattern for FIPS enforcement
4. **Performance warnings**: Warn about Argon2id unlock times (2-8 seconds with 256MB-1GB)

---

## Implementation Plan

### Task 1: Update Preferences Dialog UI Copy ⚙️

**Objective**: Update existing username hashing section to clarify it applies to BOTH username AND password.

**File**: `src/ui/dialogs/PreferencesDialog.cc`

**Changes**:

#### 1.1 Update Section Title
```cpp
// OLD:
auto* username_hash_title = Gtk::make_managed<Gtk::Label>("Username Hashing Algorithm (New Vaults Only)");

// NEW:
auto* username_hash_title = Gtk::make_managed<Gtk::Label>(
    "Key Derivation Algorithm (New Vaults Only)");
```

**Rationale**: More accurate - this setting controls cryptographic key derivation for both username hashing AND master password KEK derivation.

#### 1.2 Update Section Description
```cpp
// OLD:
auto* username_hash_desc = Gtk::make_managed<Gtk::Label>(
    "Choose how usernames are stored in newly created vault files");

// NEW:
auto* username_hash_desc = Gtk::make_managed<Gtk::Label>(
    "Choose the cryptographic algorithm for securing usernames and master passwords in newly created vaults");
```

#### 1.3 Update Algorithm Info Labels

**Current `update_username_hash_info()` function**: Already displays info for SHA3, PBKDF2, Argon2id.

**Enhancement**: Add clarification about KEK derivation mapping.

```cpp
void PreferencesDialog::update_username_hash_info() noexcept {
    const auto algorithm = m_username_hash_combo.get_active_id();

    if (algorithm == "plaintext") {
        m_username_hash_info.set_markup(
            "<span size='small' foreground='#e01b24'>⚠️  <b>DEPRECATED:</b> Usernames stored in plain text. "
            "Not recommended for security. Use for legacy compatibility only.</span>");
    } else if (algorithm == "sha3-256") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-256 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "sha3-384") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-384 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "sha3-512") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> SHA3-512 (fast, FIPS-approved)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (600K iterations)\n"
            "    Passwords automatically protected with stronger algorithm.</span>");
    } else if (algorithm == "pbkdf2-sha256") {
        m_username_hash_info.set_markup(
            "<span size='small'>ℹ️  <b>Username:</b> PBKDF2-SHA256 (configurable iterations)\n"
            "    <b>Password KEK:</b> PBKDF2-SHA256 (same parameters)\n"
            "    Consistent security for both username and password. FIPS-approved.</span>");
    } else if (algorithm == "argon2id") {
        m_username_hash_info.set_markup(
            "<span size='small' foreground='#f57900'>⚠️  <b>Non-FIPS Vault:</b> "
            "<b>Username:</b> Argon2id (memory-hard, configurable)\n"
            "    <b>Password KEK:</b> Argon2id (same parameters)\n"
            "    <b>⚠️  Vaults created with this algorithm are NOT FIPS-140-3 compliant.</b>\n"
            "    Maximum security but slower unlock (2-8 seconds).</span>");
    } else {
        m_username_hash_info.set_text("Unknown algorithm");
    }

    // FIPS mode enforcement check
    if (m_settings) {
        const bool fips_enabled = m_settings->get_boolean("fips-mode-enabled");
        if (fips_enabled && (algorithm == "plaintext" || algorithm == "argon2id")) {
            m_username_hash_info.set_markup(
                "<span size='small' foreground='#e01b24'>⚠️  <b>FIPS MODE ACTIVE:</b> "
                "This algorithm is not FIPS-approved and cannot be used. "
                "Please select a FIPS-approved algorithm (SHA3-256/384/512 or PBKDF2).</span>");
        }
    }
}
```

**Key Changes**:
- Split display into "Username" and "Password KEK" lines
- Clarify SHA3 automatic upgrade to PBKDF2 for passwords
- **Prominently warn that Argon2id creates NON-FIPS-compliant vaults**
- Warn about Argon2id unlock performance (2-8 seconds)
- **Enhanced FIPS enforcement**: Make Argon2id unselectable when FIPS mode enabled

---

#### 1.4 Add FIPS Mode Enforcement for Argon2id Selection

**Objective**: Prevent users from selecting Argon2id when FIPS mode is enabled.

**Pattern**: Follow existing username hashing FIPS enforcement in `on_username_hash_changed()` and `load_settings()`.

**File**: `src/ui/dialogs/PreferencesDialog.cc`

**Implementation**:

```cpp
// Add to on_username_hash_changed() handler
void PreferencesDialog::on_username_hash_changed() noexcept {
    update_username_hash_info();
    update_username_hash_advanced_params();

    // NEW: FIPS mode enforcement
    if (m_settings) {
        const bool fips_enabled = m_settings->get_boolean("fips-mode-enabled");
        const auto algorithm = m_username_hash_combo.get_active_id();

        // If FIPS mode enabled and user somehow selected non-FIPS algorithm,
        // revert to SHA3-256 default
        if (fips_enabled && (algorithm == "plaintext" || algorithm == "argon2id")) {
            KeepTower::Log::warning("FIPS mode active: Cannot select {} algorithm, "
                                   "reverting to SHA3-256", algorithm);
            m_username_hash_combo.set_active_id("sha3-256");
            return;  // Will trigger this handler again with valid selection
        }
    }
}

// Add to load_settings() - disable Argon2id if FIPS mode enabled
void PreferencesDialog::load_settings() {
    // ... existing code ...

    // Load FIPS mode
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

    // NEW: If FIPS mode enabled and saved algorithm is non-FIPS, use SHA3-256 default
    if (fips_enabled && (username_hash_algorithm == "plaintext" || username_hash_algorithm == "argon2id")) {
        KeepTower::Log::info("FIPS mode enabled: Overriding saved algorithm {} with SHA3-256",
                            username_hash_algorithm);
        username_hash_algorithm = "sha3-256";
    }

    m_username_hash_combo.set_active_id(username_hash_algorithm);

    // ... rest of existing code ...
}
```

**Alternative Approach** (more user-visible): Remove Argon2id from dropdown when FIPS mode enabled

```cpp
// In setup_vault_security_page(), make Argon2id addition conditional
#ifdef ENABLE_ARGON2
    // Only add Argon2id if FIPS mode is NOT enabled
    // Note: This requires reloading dropdown when FIPS mode toggled
    if (!m_settings || !m_settings->get_boolean("fips-mode-enabled")) {
        m_username_hash_combo.append("argon2id", "Argon2id (non-FIPS)");
    }
#endif

// Add signal handler for FIPS mode checkbox to reload algorithm dropdown
void PreferencesDialog::on_fips_mode_toggled() noexcept {
    // Rebuild algorithm dropdown based on new FIPS state
    rebuild_algorithm_dropdown();
}
```

**Recommended Approach**: Use both strategies:
1. Don't add Argon2id to dropdown if FIPS mode enabled at startup
2. Validate selection in `on_username_hash_changed()` as defense-in-depth
3. Show prominent warning in info label if non-FIPS algorithm somehow selected

---

### Task 2: Display Current Vault's KEK Algorithm (Read-Only) 📊

**Objective**: Show current vault's KEK algorithm when vault is open (similar to password history display).

**Pattern**: Reuse existing `update_vault_password_history_ui()` pattern.

**File**: `src/ui/dialogs/PreferencesDialog.cc`

#### 2.1 Add Current Vault Info Section

**Location**: In `setup_vault_security_page()`, after the username hashing section.

```cpp
// ========================================================================
// Current Vault KEK Algorithm (Read-Only, shown when vault open)
// ========================================================================
m_current_vault_kek_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
m_current_vault_kek_box->set_margin_top(18);

auto* current_kek_title = Gtk::make_managed<Gtk::Label>("Current Vault Security");
current_kek_title->set_halign(Gtk::Align::START);
current_kek_title->add_css_class("heading");
m_current_vault_kek_box->append(*current_kek_title);

auto* current_kek_desc = Gtk::make_managed<Gtk::Label>(
    "Security settings of the currently open vault (read-only)");
current_kek_desc->set_halign(Gtk::Align::START);
current_kek_desc->add_css_class("dim-label");
current_kek_desc->set_wrap(true);
current_kek_desc->set_max_width_chars(60);
m_current_vault_kek_box->append(*current_kek_desc);

// Username Hash Algorithm (current vault)
m_current_username_hash_label.set_halign(Gtk::Align::START);
m_current_username_hash_label.set_margin_top(12);
m_current_vault_kek_box->append(m_current_username_hash_label);

// KEK Derivation Algorithm (current vault)
m_current_kek_label.set_halign(Gtk::Align::START);
m_current_kek_label.set_margin_top(6);
m_current_vault_kek_box->append(m_current_kek_label);

// Parameters display (conditionally shown for PBKDF2/Argon2id)
m_current_kek_params_label.set_halign(Gtk::Align::START);
m_current_kek_params_label.add_css_class("dim-label");
m_current_kek_params_label.set_margin_top(6);
m_current_kek_params_label.set_margin_start(12);
m_current_vault_kek_box->append(m_current_kek_params_label);

// Initially hidden (shown when vault opens)
m_current_vault_kek_box->set_visible(false);

m_vault_security_box.append(*m_current_vault_kek_box);
```

#### 2.2 Update Header Declarations

**File**: `src/ui/dialogs/PreferencesDialog.h`

Add member variables (around line 340, after existing vault-related members):

```cpp
// Current vault KEK algorithm display (read-only)
Gtk::Box* m_current_vault_kek_box;                    ///< Container for current vault KEK info
Gtk::Label m_current_username_hash_label;             ///< Shows current vault's username hash algorithm
Gtk::Label m_current_kek_label;                       ///< Shows current vault's KEK derivation algorithm
Gtk::Label m_current_kek_params_label;                ///< Shows algorithm parameters (iterations/memory)
```

#### 2.3 Implement `update_current_vault_kek_info()` Method

**File**: `src/ui/dialogs/PreferencesDialog.cc`

```cpp
void PreferencesDialog::update_current_vault_kek_info() noexcept {
    // Check if vault is open
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        // No vault open - hide current vault info
        if (m_current_vault_kek_box) {
            m_current_vault_kek_box->set_visible(false);
        }
        return;
    }

    // Vault open - show current vault info
    if (m_current_vault_kek_box) {
        m_current_vault_kek_box->set_visible(true);
    }

    // Get vault security policy
    const auto policy_opt = m_vault_manager->get_vault_security_policy();
    if (!policy_opt) {
        m_current_username_hash_label.set_markup(
            "<span>Username Algorithm: <b>N/A</b></span>");
        m_current_kek_label.set_markup(
            "<span>Password KEK Algorithm: <b>N/A</b></span>");
        m_current_kek_params_label.set_text("");
        return;
    }

    const auto& policy = *policy_opt;

    // Display username hash algorithm
    std::string username_algo_display;
    switch (policy.username_hash_algorithm) {
        case 0x00: username_algo_display = "Plaintext (DEPRECATED)"; break;
        case 0x01: username_algo_display = "SHA3-256 (FIPS)"; break;
        case 0x02: username_algo_display = "SHA3-384 (FIPS)"; break;
        case 0x03: username_algo_display = "SHA3-512 (FIPS)"; break;
        case 0x04: username_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)"; break;
        case 0x05: username_algo_display = "Argon2id (⚠️ non-FIPS vault)"; break;
        default: username_algo_display = "Unknown (" +
            std::to_string(policy.username_hash_algorithm) + ")";
    }

    m_current_username_hash_label.set_markup(
        "<span>Username Algorithm: <b>" + username_algo_display + "</b></span>");

    // Get KEK algorithm from first KeySlot (all slots share same algorithm)
    // Note: VaultManager needs a method to expose this
    // For now, infer from policy (same as username for PBKDF2/Argon2id, PBKDF2 for SHA3)
    std::string kek_algo_display;
    std::string params_display;

    if (policy.username_hash_algorithm >= 0x01 && policy.username_hash_algorithm <= 0x03) {
        // SHA3 variants → KEK uses PBKDF2
        kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
        params_display = std::to_string(policy.pbkdf2_iterations) + " iterations";
    } else if (policy.username_hash_algorithm == 0x04) {
        // PBKDF2
        kek_algo_display = "PBKDF2-HMAC-SHA256 (FIPS)";
        params_display = std::to_string(policy.pbkdf2_iterations) + " iterations";
    } else if (policy.username_hash_algorithm == 0x05) {
        // Argon2id
        kek_algo_display = "Argon2id (⚠️ non-FIPS vault)";
        params_display = std::to_string(policy.argon2_memory_kb) + " KB memory, " +
                        std::to_string(policy.argon2_iterations) + " time cost, " +
                        std::to_string(policy.argon2_parallelism) + " threads";
    } else {
        // Plaintext or unknown
        kek_algo_display = "PBKDF2-HMAC-SHA256 (default fallback)";
        params_display = "600,000 iterations";
    }

    m_current_kek_label.set_markup(
        "<span>Password KEK Algorithm: <b>" + kek_algo_display + "</b></span>");
    m_current_kek_params_label.set_text("  Parameters: " + params_display);
}
```

#### 2.4 Add Method Declaration to Header

**File**: `src/ui/dialogs/PreferencesDialog.h`

Add around line 45, after `update_username_hash_info()`:

```cpp
void update_current_vault_kek_info() noexcept;       ///< Update current vault KEK algorithm display
```

#### 2.5 Call from `on_dialog_shown()`

**File**: `src/ui/dialogs/PreferencesDialog.cc`

```cpp
void PreferencesDialog::on_dialog_shown() noexcept {
    // Lazy load vault password history UI only once (expensive operation if many users)
    if (!m_history_ui_loaded) {
        m_history_ui_loaded = true;
        update_vault_password_history_ui();
    }

    // Update current vault KEK info (always refresh, vault may have changed)
    update_current_vault_kek_info();
}
```

**Alternative Enhancement**: Also call `update_current_vault_kek_info()` whenever vault state changes (requires VaultManager signal).

---

### Task 3: Add Help Documentation 📚

**Objective**: Provide comprehensive help documentation about KEK derivation algorithms.

**File**: `docs/help/user/05-vault-security-algorithms.md` (NEW)

#### 3.1 Create Help Page

```markdown
# Vault Security Algorithms

KeepTower uses cryptographic algorithms to protect your vault data in two ways:

1. **Username Hashing**: Hides user identities in the vault file
2. **Master Password KEK Derivation**: Protects your master password from brute-force attacks

## Algorithm Selection

When creating a vault, you select one algorithm preference. KeepTower intelligently applies it:

### SHA3 Variants (SHA3-256, SHA3-384, SHA3-512)

**Best for**: Fast vault access with FIPS compliance

**How it works**:
- **Username**: SHA3 hash (very fast, ~1 microsecond)
- **Master Password**: Automatically upgraded to PBKDF2 (600,000 iterations, ~1 second)

**Why the difference?**
SHA3 is a hash function designed for speed, making it perfect for usernames (which are identifiers, not secrets). However, passwords MUST be protected with a slow key derivation function (KDF) to resist brute-force attacks. If we used SHA3 for passwords, attackers could test millions of passwords per second. PBKDF2 slows this down to ~1 password per second.

**Security**: ✅ Excellent (FIPS-approved, automatic password upgrade)
**Speed**: ✅ Very fast unlock (~1 second)
**FIPS**: ✅ Approved

---

### PBKDF2-HMAC-SHA256

**Best for**: Consistent security with broad compatibility

**How it works**:
- **Username**: PBKDF2 hash (configurable iterations, 10K-1M)
- **Master Password**: PBKDF2 KEK (same iterations)

**Why use this?**
Provides consistent protection for both usernames and passwords using the same industry-standard algorithm. Widely supported and FIPS-approved.

**Security**: ✅ Excellent (FIPS-approved, proven track record)
**Speed**: ⚠️ Moderate (1-2 seconds with 600K iterations)
**FIPS**: ✅ Approved

**Tuning**: Increase iterations for stronger security (slower unlock).

---

### Argon2id

**Best for**: Maximum security against modern attacks (GPU/ASIC resistance)

**How it works**:
- **Username**: Argon2id hash (configurable memory/time)
- **Master Password**: Argon2id KEK (same parameters)

**Why use this?**
Argon2id is memory-hard, meaning it requires large amounts of RAM to compute. This makes it extremely expensive for attackers using GPUs or specialized hardware (ASICs). Winner of the Password Hashing Competition (2015).

**Security**: ✅✅ Maximum (memory-hard, GPU-resistant)
**Speed**: ❌ Slow (2-8 seconds depending on memory setting)
**FIPS**: ❌ Not approved (but cryptographically superior)

**Tuning**: Increase memory cost for stronger security (slower unlock).

**⚠️ Trade-off**: Not approved for FIPS mode. If FIPS compliance is required, use PBKDF2 or SHA3.

---

## Algorithm Comparison

| Algorithm | Username | Password KEK | Unlock Time | FIPS | Best For |
|-----------|----------|--------------|-------------|------|----------|
| SHA3-256 | SHA3-256 | PBKDF2 600K | ~1 sec | ✅ | General use |
| SHA3-384 | SHA3-384 | PBKDF2 600K | ~1 sec | ✅ | General use |
| SHA3-512 | SHA3-512 | PBKDF2 600K | ~1 sec | ✅ | General use |
| PBKDF2 | PBKDF2 | PBKDF2 | 1-2 sec | ✅ | Consistency |
| Argon2id | Argon2id | Argon2id | 2-8 sec | ❌ | Max security |

---

## Security Recommendations

### For Most Users
**Choose**: SHA3-256 (default)
**Reason**: Fast unlock, FIPS-approved, automatic password upgrade

### For High-Security Environments
**Choose**: Argon2id with 128-256 MB memory
**Reason**: Maximum protection against well-funded attackers with GPU farms

### For FIPS-Required Environments
**Choose**: PBKDF2-HMAC-SHA256 with 600K+ iterations
**Reason**: FIPS-approved, consistent algorithm for all operations

### For Regulatory Compliance
**Choose**: SHA3-512 or PBKDF2-SHA256
**Reason**: Government-approved algorithms (FIPS 140-3)

---

## Frequently Asked Questions

**Q: Can I change the algorithm on an existing vault?**
A: No. The algorithm is set at vault creation and cannot be changed. This prevents security downgrade attacks. To change algorithms, create a new vault and migrate data.

**Q: Why does SHA3 automatically upgrade to PBKDF2 for passwords?**
A: SHA3 is a hash function, not a key derivation function. It's too fast for password protection (~1 microsecond = millions of attempts/second). PBKDF2 intentionally slows down password testing to ~1/second, making brute-force attacks impractical.

**Q: Is Argon2id safe to use?**
A: Yes! Argon2id is cryptographically superior to PBKDF2. It's not FIPS-approved because FIPS certification is expensive and time-consuming, not because of security concerns. Many security-critical applications use Argon2id.

**Q: What iterations/memory should I use?**
A: Defaults are secure for most users:
- PBKDF2: 600,000 iterations (NIST recommended)
- Argon2id: 64 MB memory, 3 time cost

Increase for stronger security (longer unlock times).

**Q: Does this affect vault file compatibility?**
A: Each vault stores its algorithm in the file header. KeepTower automatically uses the correct algorithm when opening any vault. You can have multiple vaults with different algorithms.

---

## Technical Details

### Why Not SHA3 for Passwords?

SHA3 is a cryptographic hash function optimized for:
- Speed (nanosecond computation)
- Collision resistance
- Data integrity verification

Key Derivation Functions (KDFs) like PBKDF2/Argon2id are optimized for:
- Slowness (intentional computational cost)
- Memory hardness (resist parallelization)
- Password security

**Attack Scenario**:
```
SHA3-256 password hashing:
- Attacker with RTX 4090 GPU: ~50,000,000 hashes/second
- 8-character password (lowercase+numbers): 36^8 = 2.8 trillion combinations
- Time to crack: 2.8 trillion / 50 million = 15.6 hours

PBKDF2-HMAC-SHA256 (600K iterations):
- Same attacker: ~100 hashes/second
- Same password space: 2.8 trillion combinations
- Time to crack: 2.8 trillion / 100 = 888 years

Argon2id (256 MB memory):
- Same attacker: ~10 hashes/second (memory bottleneck)
- Same password space: 2.8 trillion combinations
- Time to crack: 2.8 trillion / 10 = 8,880 years
```

This is why KeepTower automatically upgrades SHA3 selections to PBKDF2 for password protection.

---

## See Also

- [Security Best Practices](06-security-best-practices.md)
- [FIPS Mode](07-fips-mode.md)
- [Vault Format Specification](../../developer/VAULT_FORMAT.md)
```

#### 3.2 Update Help Index

**File**: `docs/help/user/README.md`

Add entry:
```markdown
5. [Vault Security Algorithms](05-vault-security-algorithms.md) - Understanding KEK derivation
```

#### 3.3 Integrate Help Button in Preferences

**File**: `src/ui/dialogs/PreferencesDialog.cc`

Add help button next to algorithm selection:

```cpp
// Help button (links to documentation)
auto* help_button = Gtk::make_managed<Gtk::Button>();
help_button->set_icon_name("help-about-symbolic");
help_button->set_tooltip_text("Learn about KEK derivation algorithms");
help_button->signal_clicked().connect([this]() {
    auto& help = Utils::HelpManager::get_instance();
    help.open_help(Utils::HelpTopic::VaultSecurityAlgorithms, *this);
});
username_hash_row->append(*help_button);
```

#### 3.4 Add HelpTopic Enum Value

**File**: `src/utils/helpers/HelpManager.h`

Add to `enum class HelpTopic`:

```cpp
VaultSecurityAlgorithms,  ///< KEK derivation and username hashing algorithms
```

Update topic mapping in `HelpManager.cc`:

```cpp
case HelpTopic::VaultSecurityAlgorithms:
    return "user/05-vault-security-algorithms.html";
```

---

### Task 4: Update Advanced Parameters Help Text 📝

**Objective**: Clarify that advanced parameters apply to both username AND password KEK.

**File**: `src/ui/dialogs/PreferencesDialog.cc`

**Current code** (around line 680):

```cpp
m_username_hash_advanced_help.set_markup(
    "<span size='small'>⚠️  These parameters apply to newly created vaults. "
    "Higher values increase security but slow down vault operations.</span>");
```

**Updated code**:

```cpp
m_username_hash_advanced_help.set_markup(
    "<span size='small'>⚠️  These parameters apply to <b>both username hashing and password KEK derivation</b> "
    "in newly created vaults. Higher values increase security but slow down vault unlock.</span>");
```

---

### Task 5: Add Performance Warning for Argon2id ⚠️

**Objective**: Warn users about Argon2id unlock times before they create a vault.

**File**: `src/ui/dialogs/PreferencesDialog.cc`

**Add to `update_username_hash_advanced_params()` function**:

```cpp
void PreferencesDialog::update_username_hash_advanced_params() noexcept {
    const auto algorithm = m_username_hash_combo.get_active_id();

    if (algorithm == "pbkdf2-sha256") {
        // Show PBKDF2 parameters
        m_pbkdf2_iterations_box->set_visible(true);
        m_argon2_memory_box->set_visible(false);
        m_argon2_time_box->set_visible(false);
    } else if (algorithm == "argon2id") {
        // Show Argon2 parameters
        m_pbkdf2_iterations_box->set_visible(false);
        m_argon2_memory_box->set_visible(true);
        m_argon2_time_box->set_visible(true);

        // NEW: Add performance warning based on current memory setting
        uint32_t memory_kb = m_argon2_memory_spin.get_value_as_int();
        uint32_t time_cost = m_argon2_time_spin.get_value_as_int();

        double estimated_time = estimate_argon2_time(memory_kb, time_cost);

        if (estimated_time > 3.0) {
            m_username_hash_advanced_help.set_markup(
                "<span size='small' foreground='#f57900'>⚠️  <b>Performance Warning:</b> "
                "Estimated vault unlock time with these settings: <b>" +
                std::to_string(static_cast<int>(estimated_time)) + " seconds</b>. "
                "Consider reducing memory or time cost for faster unlocking.</span>");
        } else {
            m_username_hash_advanced_help.set_markup(
                "<span size='small'>⚠️  These parameters apply to <b>both username hashing and password KEK derivation</b> "
                "in newly created vaults. Estimated unlock time: ~" +
                std::to_string(static_cast<int>(estimated_time)) + " seconds.</span>");
        }
    } else {
        // Hide all advanced parameters for SHA3
        m_pbkdf2_iterations_box->set_visible(false);
        m_argon2_memory_box->set_visible(false);
        m_argon2_time_box->set_visible(false);
    }
}

// Helper function (add to PreferencesDialog private methods)
double PreferencesDialog::estimate_argon2_time(uint32_t memory_kb, uint32_t time_cost) const noexcept {
    // Rough estimation based on benchmarks (adjust to actual hardware)
    // Formula: (memory_kb / 65536) * time_cost * 0.5 seconds
    return (static_cast<double>(memory_kb) / 65536.0) *
           static_cast<double>(time_cost) * 0.5;
}
```

**Add method declaration to header**:

```cpp
double estimate_argon2_time(uint32_t memory_kb, uint32_t time_cost) const noexcept;
```

---

### Task 6: Testing & Validation ✅

#### 6.1 Manual Testing Checklist

**Test Case 1: SHA3 Algorithm Display**
- [ ] Open Preferences → Vault Security
- [ ] Select "SHA3-256 (FIPS)"
- [ ] Verify info label shows: "Username: SHA3-256" and "Password KEK: PBKDF2"
- [ ] Create vault, verify unlock time ~1 second

**Test Case 2: PBKDF2 Algorithm Display**
- [ ] Select "PBKDF2-SHA256 (FIPS)"
- [ ] Verify info label shows both use PBKDF2
- [ ] Expand advanced parameters, set 1M iterations
- [ ] Create vault, verify unlock time ~1.7 seconds

**Test Case 3: Argon2id Performance Warning**
- [ ] Select "Argon2id (non-FIPS)"
- [ ] Expand advanced parameters
- [ ] Set memory to 256 MB, time cost to 5
- [ ] Verify warning shows "~2-3 seconds" unlock time
- [ ] Set memory to 1 GB, time cost to 10
- [ ] Verify warning shows ">5 seconds" unlock time

**Test Case 4: Current Vault Display**
- [ ] Create vault with SHA3-256
- [ ] Open Preferences while vault open
- [ ] Verify "Current Vault Security" section visible
- [ ] Verify shows: Username=SHA3-256, KEK=PBKDF2-SHA256
- [ ] Close vault
- [ ] Reopen Preferences
- [ ] Verify "Current Vault Security" section hidden

**Test Case 5: FIPS Mode Enforcement**
- [ ] Enable FIPS mode
- [ ] Open Preferences
- [ ] Verify Argon2id option disabled or shows warning
- [ ] Verify SHA3/PBKDF2 options available

**Test Case 6: Help Documentation**
- [ ] Click help button (ℹ️) next to algorithm dropdown
- [ ] Verify opens help page in browser
- [ ] Verify help page explains SHA3→PBKDF2 automatic upgrade
- [ ] Verify algorithm comparison table accurate

#### 6.2 Automated Tests (Optional)

**Unit Tests** (if UI testing framework available):

```cpp
TEST(PreferencesDialogTest, SHA3ShowsPBKDF2UpgradeMessage) {
    PreferencesDialog dialog(window, vault_manager);
    dialog.set_algorithm("sha3-256");

    Glib::ustring info = dialog.get_username_hash_info_text();

    EXPECT_TRUE(info.find("PBKDF2") != Glib::ustring::npos);
    EXPECT_TRUE(info.find("automatically") != Glib::ustring::npos);
}

TEST(PreferencesDialogTest, CurrentVaultKekDisplayedWhenVaultOpen) {
    vault_manager->create_vault("test.vault", "user", "pass");

    PreferencesDialog dialog(window, vault_manager);

    EXPECT_TRUE(dialog.is_current_vault_kek_box_visible());
    EXPECT_NE(dialog.get_current_kek_label_text(), "N/A");
}

TEST(PreferencesDialogTest, Argon2idShowsPerformanceWarning) {
    PreferencesDialog dialog(window, vault_manager);
    dialog.set_algorithm("argon2id");
    dialog.set_argon2_memory_kb(262144);  // 256 MB
    dialog.set_argon2_time_cost(5);

    Glib::ustring info = dialog.get_username_hash_info_text();

    EXPECT_TRUE(info.find("Performance Warning") != Glib::ustring::npos);
    EXPECT_TRUE(info.find("seconds") != Glib::ustring::npos);
}
```

---

## Implementation Checklist

### UI Changes
- [ ] Update section title: "Key Derivation Algorithm (New Vaults Only)"
- [ ] Update section description to mention both username and password
- [ ] Update `update_username_hash_info()` to show Username/Password KEK split
- [ ] Add SHA3→PBKDF2 automatic upgrade messaging
- [ ] Add Argon2id performance warnings (2-8 seconds)
- [ ] Update advanced parameters help text

### Current Vault Display
- [ ] Add `m_current_vault_kek_box` container
- [ ] Add `m_current_username_hash_label`
- [ ] Add `m_current_kek_label`
- [ ] Add `m_current_kek_params_label`
- [ ] Implement `update_current_vault_kek_info()` method
- [ ] Call from `on_dialog_shown()`
- [ ] Show/hide based on vault open state

### Help Documentation
- [ ] Create `docs/help/user/05-vault-security-algorithms.md`
- [ ] Add SHA3 vs KDF explanation
- [ ] Add algorithm comparison table
- [ ] Add security recommendations
- [ ] Add FAQ section
- [ ] Add technical attack scenario examples
- [ ] Update help index
- [ ] Add help button to Preferences dialog
- [ ] Add `HelpTopic::VaultSecurityAlgorithms` enum value
- [ ] Update `HelpManager` topic mapping

### Performance Warnings
- [ ] Implement `estimate_argon2_time()` helper
- [ ] Update `update_username_hash_advanced_params()` to show warnings
- [ ] Connect Argon2 spinbutton signals to update warnings live

### Testing
- [ ] Manual test SHA3 display
- [ ] Manual test PBKDF2 display
- [ ] Manual test Argon2id display and warnings
- [ ] Manual test current vault display (vault open/closed)
- [ ] Manual test FIPS mode enforcement
- [ ] Manual test help button
- [ ] Manual test unlock times match estimates

---

## Success Criteria

✅ **Functional Requirements**:
1. Preferences UI clearly shows algorithm applies to both username AND password
2. SHA3 selections display "automatic PBKDF2 upgrade" message
3. Current vault's KEK algorithm displayed when vault open
4. Argon2id shows performance warnings for high memory settings
5. Help documentation explains KEK derivation and automatic upgrades

✅ **User Experience Requirements**:
1. Users understand SHA3→PBKDF2 automatic upgrade (not confused)
2. Users aware of Argon2id performance trade-offs before vault creation
3. Users can see current vault's security settings (read-only)
4. Help documentation answers common questions

✅ **Code Quality Requirements**:
1. SRP compliance (PreferencesDialog = UI only, no business logic)
2. Follows existing patterns (username hash UI, vault password history display)
3. No changes to completed backend code (KekDerivationService, VaultManager)
4. CONTRIBUTING.md compliant (error handling, FIPS enforcement)

✅ **Documentation Requirements**:
1. Comprehensive help page with examples
2. Algorithm comparison table
3. Security recommendations by use case
4. FAQ addressing common concerns

---

## Implementation Timeline

**Estimated Effort**: 3-4 days (1 developer)

| Task | Estimated Time | Priority |
|------|---------------|----------|
| Task 1: Update Preferences UI copy | 2-3 hours | High |
| Task 2: Current vault KEK display | 4-5 hours | High |
| Task 3: Help documentation | 6-8 hours | High |
| Task 4: Advanced parameters help | 30 minutes | Medium |
| Task 5: Performance warnings | 2-3 hours | Medium |
| Task 6: Testing & validation | 4-6 hours | High |

**Total**: ~20-25 hours

---

## Risks & Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Users confused by SHA3→PBKDF2 upgrade | Medium | Medium | Clear messaging in UI and help docs |
| Argon2id performance surprises users | Low | Low | Prominent warnings before vault creation |
| Help page too technical | Low | Medium | Add "Simple Explanation" section at top |
| Current vault display shows wrong algorithm | High | Low | Thorough testing with all algorithm types |

---

## Future Enhancements (Out of Scope for Phase 4)

1. **Live Unlock Time Estimation**: Real-time benchmark on user's hardware
2. **Algorithm Upgrade Wizard**: Migrate existing vaults to new algorithms
3. **Performance Profiles**: Preset configurations (Fast/Balanced/Secure)
4. **Multi-Vault Algorithm Report**: Show algorithms for all vaults in one view

---

## References

- [KEK Derivation Algorithm Plan](KEK_DERIVATION_ALGORITHM_PLAN.md) - Overall project plan
- [Username Hashing Preferences UI](USERNAME_HASH_PREFERENCES_UI_PLAN.md) - Similar UI pattern
- [CONTRIBUTING.md](../../CONTRIBUTING.md) - SRP and code quality standards
- [NIST SP 800-132](https://csrc.nist.gov/publications/detail/sp/800-132/final) - PBKDF2 recommendations
- [RFC 9106](https://www.rfc-editor.org/rfc/rfc9106.html) - Argon2 specification

---

**Document Status**: ✅ Ready for Implementation
**Next Steps**: Begin Task 1 (Update Preferences UI copy)
**Approval Required**: Architecture review (if adding VaultManager API methods)

---

## Appendix A: Wire frame Mockups

### Preferences Dialog - Algorithm Selection

```
┌─────────────────────────────────────────────────────────────┐
│ Preferences                                                  │
├─────────────────────────────────────────────────────────────┤
│ Sidebar     │ Vault Security                                │
│             │                                                │
│ Appearance  │ Key Derivation Algorithm (New Vaults Only)    │
│ Account     │ Choose the cryptographic algorithm for        │
│ ▸ Vault     │ securing usernames and master passwords      │
│   Security  │                                                │
│ Storage     │ Algorithm: [SHA3-256 (FIPS)           ▼]  ℹ️ │
│             │                                                │
│             │ ℹ️ Username: SHA3-256 (fast, FIPS-approved)  │
│             │    Password KEK: PBKDF2-SHA256 (600K iter)    │
│             │    Passwords automatically protected with     │
│             │    stronger algorithm.                        │
│             │                                                │
│             │ ⚠️ This setting only affects newly created   │
│             │    vaults. Existing vaults continue to use   │
│             │    their original algorithm.                  │
│             │                                                │
│             │ [ Advanced Parameters ▼ ]                    │
│             │                                                │
│             │ ────────────────────────────────────────────  │
│             │                                                │
│             │ Current Vault Security                        │
│             │ Security settings of the currently open vault │
│             │                                                │
│             │ Username Algorithm: SHA3-256 (FIPS)           │
│             │ Password KEK Algorithm: PBKDF2-HMAC-SHA256    │
│             │   Parameters: 600,000 iterations              │
│             │                                                │
└─────────────────────────────────────────────────────────────┘
```

### Preferences Dialog - Argon2id with Warning

```
┌─────────────────────────────────────────────────────────────┐
│ Preferences                                                  │
├─────────────────────────────────────────────────────────────┤
│ Sidebar     │ Vault Security                                │
│             │                                                │
│ Appearance  │ Key Derivation Algorithm (New Vaults Only)    │
│ Account     │                                                │
│ ▸ Vault     │ Algorithm: [Argon2id (non-FIPS)       ▼]  ℹ️ │
│   Security  │                                                │
│ Storage     │ ⚠️ Non-FIPS: Username: Argon2id (memory-hard)│
│             │    Password KEK: Argon2id (same parameters)   │
│             │    Maximum security but not FIPS-approved.    │
│             │    Unlock may take 2-8 seconds.               │
│             │                                                │
│             │ [ Advanced Parameters ▲ ]                    │
│             │                                                │
│             │   Memory Cost: [256] MB                       │
│             │   Time Cost:   [  5]                          │
│             │                                                │
│             │   ⚠️ Performance Warning: Estimated vault    │
│             │      unlock time with these settings:         │
│             │      ~3 seconds                               │
│             │                                                │
└─────────────────────────────────────────────────────────────┘
```

---

**End of Phase 4 Implementation Plan**
