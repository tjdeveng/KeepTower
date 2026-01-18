# Username Hashing Algorithm Selection - Preferences UI Implementation Plan

**Date:** 2026-01-19  
**Feature:** Add UI for selecting username hashing algorithm in Preferences  
**Phase:** Username Hashing Security - Phase 2 (UI Configuration)  
**Status:** Planning

---

## Overview

Add a user-facing preference control to the Security tab of PreferencesDialog that allows administrators to select the username hashing algorithm for **new vaults only**. This follows the completed Phase 1 implementation where the core hashing infrastructure and GSettings backend are already functional.

**Important Security Note:** Algorithm selection affects **new vault creation only**. Existing vaults retain their configured algorithm (cannot be changed without migration).

---

## Design Principles (per CONTRIBUTING.md)

### Single Responsibility Principle (SRP)
- **PreferencesDialog**: Display and coordinate settings UI only
- **SettingsValidator**: Validate algorithm choices (FIPS enforcement)
- **GSettings**: Persist user's algorithm selection
- **VaultOpenHandler**: Read setting during vault creation
- **UsernameHashService**: Hash usernames (unchanged)

### Security-First Design
- FIPS-140-3 warnings for non-approved algorithms
- Clear explanation of algorithm trade-offs
- Visual indicators for security implications
- Prevent selection of Argon2id when FIPS mode enabled

---

## Current State Analysis

### ✅ Already Implemented (Phase 1)
1. **GSettings Schema** (`data/com.tjdeveng.keeptower.gschema.xml`):
   - `username-hash-algorithm` (string): Default `'sha3-256'`
   - `username-pbkdf2-iterations` (uint): Default 600000
   - `username-argon2-memory-kb` (uint): Default 65536
   - `username-argon2-iterations` (uint): Default 3

2. **Core Services**:
   - `UsernameHashService`: 5 algorithms implemented (24/24 tests passing)
   - Salt generation using OpenSSL RAND_bytes (FIPS-approved)
   - Constant-time hash comparison

3. **Integration**:
   - `VaultOpenHandler`: Reads setting, applies to new vaults
   - `VaultCreationOrchestrator`: Hashes usernames during creation
   - `VaultManagerV2`: Hash-based authentication

4. **Build System**:
   - `ENABLE_ARGON2` compile-time flag
   - Graceful degradation if libargon2 not available

### ❌ Missing (Phase 2 - This Plan)
1. **UI Controls** in PreferencesDialog Security tab:
   - Dropdown/ComboBox for algorithm selection
   - Information labels explaining each algorithm
   - Warning for non-FIPS choices
   - Advanced parameters UI (iterations, memory)

2. **Runtime Validation**:
   - Check `ENABLE_ARGON2` before showing option
   - Enforce FIPS constraints (disable Argon2id/plaintext)
   - Validate parameter ranges

3. **User Education**:
   - Tooltips explaining algorithm differences
   - Visual security strength indicators
   - "New vaults only" clarification

---

## Implementation Plan

### Step 1: Add UI Controls to PreferencesDialog (Security Tab)

**File:** `src/ui/dialogs/PreferencesDialog.cc` / `.h`

**Location:** After "Password History" section, before "Auto-lock" section

**Components:**

1. **Section Header**
   ```cpp
   // Username Hashing (New Vaults Only)
   auto* username_hash_header = Gtk::make_managed<Gtk::Label>();
   username_hash_header->set_markup("<b>Username Hashing Algorithm (New Vaults Only)</b>");
   username_hash_header->set_halign(Gtk::Align::START);
   security_grid->attach(*username_hash_header, 0, row, 2, 1);
   row++;
   ```

2. **Algorithm Selection Dropdown**
   ```cpp
   // Algorithm ComboBoxText
   m_username_hash_combo = Gtk::make_managed<Gtk::ComboBoxText>();
   
   // Populate with available algorithms (check ENABLE_ARGON2)
   m_username_hash_combo->append("sha3-256", "SHA3-256 (Recommended, FIPS-Approved)");
   m_username_hash_combo->append("sha3-384", "SHA3-384 (FIPS-Approved)");
   m_username_hash_combo->append("sha3-512", "SHA3-512 (Strongest FIPS-Approved)");
   m_username_hash_combo->append("pbkdf2-sha256", "PBKDF2-SHA256 (Slow, FIPS-Approved)");
   
   #ifdef ENABLE_ARGON2
   if (!fips_mode_enabled()) {
       m_username_hash_combo->append("argon2id", "Argon2id (Strongest, NOT FIPS-Approved)");
   }
   #endif
   
   // Load current value from GSettings
   Glib::ustring current_algo = m_settings->get_string("username-hash-algorithm");
   m_username_hash_combo->set_active_id(current_algo);
   ```

3. **Info/Warning Label**
   ```cpp
   m_username_hash_info = Gtk::make_managed<Gtk::Label>();
   m_username_hash_info->set_wrap(true);
   m_username_hash_info->set_max_width_chars(60);
   m_username_hash_info->set_halign(Gtk::Align::START);
   update_username_hash_info(); // Update based on selection
   ```

4. **Signal Handler**
   ```cpp
   m_username_hash_combo->signal_changed().connect(
       sigc::mem_fun(*this, &PreferencesDialog::on_username_hash_changed)
   );
   ```

### Step 2: Implement Helper Methods

**File:** `src/ui/dialogs/PreferencesDialog.cc`

1. **`on_username_hash_changed()`**
   ```cpp
   void PreferencesDialog::on_username_hash_changed() {
       Glib::ustring selected = m_username_hash_combo->get_active_id();
       
       // Update GSettings
       m_settings->set_string("username-hash-algorithm", selected);
       
       // Update info label
       update_username_hash_info();
       
       // Show/hide advanced parameters based on selection
       update_advanced_params_visibility();
   }
   ```

2. **`update_username_hash_info()`**
   ```cpp
   void PreferencesDialog::update_username_hash_info() {
       Glib::ustring selected = m_username_hash_combo->get_active_id();
       
       std::string info_text;
       bool show_warning = false;
       
       if (selected == "sha3-256") {
           info_text = "✓ FIPS 140-3 approved. Fast, secure, recommended for most users.";
       } else if (selected == "sha3-384") {
           info_text = "✓ FIPS 140-3 approved. Stronger than SHA3-256, slightly slower.";
       } else if (selected == "sha3-512") {
           info_text = "✓ FIPS 140-3 approved. Maximum FIPS-approved strength.";
       } else if (selected == "pbkdf2-sha256") {
           info_text = "✓ FIPS 140-3 approved. Memory-hard, slower (600k iterations). "
                      "Protects against GPU attacks.";
       } else if (selected == "argon2id") {
           info_text = "⚠ NOT FIPS-approved. Strongest algorithm available, "
                      "memory-hard (64 MB). Disables FIPS compliance mode.";
           show_warning = true;
       }
       
       info_text += "\n\n<i>Note: This setting only affects NEW vaults created after "
                   "this change. Existing vaults retain their configured algorithm.</i>";
       
       m_username_hash_info->set_markup(info_text);
       
       // Style the label based on FIPS compliance
       if (show_warning) {
           m_username_hash_info->add_css_class("warning");
       } else {
           m_username_hash_info->remove_css_class("warning");
       }
   }
   ```

3. **`update_advanced_params_visibility()`** (Optional for Phase 2)
   ```cpp
   void PreferencesDialog::update_advanced_params_visibility() {
       Glib::ustring selected = m_username_hash_combo->get_active_id();
       
       // Show PBKDF2 iterations control if PBKDF2 selected
       bool show_pbkdf2_params = (selected == "pbkdf2-sha256");
       m_pbkdf2_iterations_box->set_visible(show_pbkdf2_params);
       
       // Show Argon2 memory control if Argon2id selected
       bool show_argon2_params = (selected == "argon2id");
       m_argon2_memory_box->set_visible(show_argon2_params);
   }
   ```

### Step 3: Add Member Variables

**File:** `src/ui/dialogs/PreferencesDialog.h`

```cpp
class PreferencesDialog : public Gtk::Dialog {
private:
    // ... existing members ...
    
    // Username Hashing UI (Phase 2)
    Gtk::ComboBoxText* m_username_hash_combo;
    Gtk::Label* m_username_hash_info;
    
    // Optional: Advanced parameters (can defer to Phase 3)
    Gtk::Box* m_pbkdf2_iterations_box;
    Gtk::SpinButton* m_pbkdf2_iterations_spin;
    Gtk::Box* m_argon2_memory_box;
    Gtk::SpinButton* m_argon2_memory_spin;
    
    // Signal handlers
    void on_username_hash_changed();
    void update_username_hash_info();
    void update_advanced_params_visibility();
};
```

### Step 4: Add CSS Styling (Optional)

**File:** `src/ui/styles.css` or inline in PreferencesDialog constructor

```css
.warning {
    color: #ff6b00; /* Orange warning color */
    font-weight: bold;
}

.info-box {
    background-color: rgba(100, 149, 237, 0.1);
    border-left: 3px solid #6495ED;
    padding: 8px;
    margin: 4px 0;
}
```

### Step 5: FIPS Enforcement (SettingsValidator)

**File:** `src/utils/SettingsValidator.h` / `.cc`

Add validation method:

```cpp
class SettingsValidator {
public:
    // ... existing methods ...
    
    /**
     * @brief Validate username hashing algorithm selection
     * @param settings GSettings object
     * @param fips_mode_enabled Whether FIPS mode is active
     * @return Validated algorithm string (may differ from input if invalid)
     */
    static Glib::ustring validate_username_hash_algorithm(
        const Glib::RefPtr<Gio::Settings>& settings,
        bool fips_mode_enabled
    ) {
        Glib::ustring algo = settings->get_string("username-hash-algorithm");
        
        // FIPS mode: Block non-approved algorithms
        if (fips_mode_enabled) {
            if (algo == "argon2id" || algo == "plaintext") {
                Log::warning("SettingsValidator: '{}' not allowed in FIPS mode, "
                            "reverting to 'sha3-256'", algo);
                settings->set_string("username-hash-algorithm", "sha3-256");
                return "sha3-256";
            }
        }
        
        // Check if Argon2 is available
        #ifndef ENABLE_ARGON2
        if (algo == "argon2id") {
            Log::warning("SettingsValidator: Argon2id not available "
                        "(libargon2 not installed), reverting to 'sha3-256'");
            settings->set_string("username-hash-algorithm", "sha3-256");
            return "sha3-256";
        }
        #endif
        
        return algo;
    }
};
```

### Step 6: Testing Strategy

**Unit Tests** (`tests/test_preferences_ui.cc` - new file):

```cpp
TEST(PreferencesDialogTest, UsernameHashAlgorithmSelection) {
    // Test algorithm dropdown population
    // Test GSettings persistence
    // Test FIPS enforcement
    // Test Argon2 availability check
}

TEST(SettingsValidatorTest, UsernameHashAlgorithmValidation) {
    // Test FIPS mode blocks Argon2id
    // Test missing ENABLE_ARGON2 blocks Argon2id
    // Test valid algorithms pass through
}
```

**Manual Testing Checklist:**

- [ ] Algorithm dropdown populates correctly
- [ ] Selected algorithm persists across dialog closes
- [ ] Info label updates when selection changes
- [ ] FIPS mode hides Argon2id option
- [ ] Non-ENABLE_ARGON2 builds hide Argon2id
- [ ] New vaults created with selected algorithm
- [ ] Existing vaults unaffected by preference change
- [ ] Warning message appears for non-FIPS algorithms

---

## Integration Points

### 1. VaultOpenHandler (Already Integrated ✅)
- Reads `username-hash-algorithm` from GSettings
- Applies to `VaultSecurityPolicy` during vault creation
- No changes needed

### 2. PreferencesDialog (This PR)
- Add UI controls to Security tab
- Wire up signal handlers
- Implement validation logic

### 3. SettingsValidator (Enhancement)
- Add validation method for algorithm selection
- Enforce FIPS constraints
- Handle build-time availability checks

---

## Algorithm Reference Table (for UI Documentation)

| Algorithm        | Hash Size | FIPS-Approved | Speed   | Security Level | Use Case                          |
|------------------|-----------|---------------|---------|----------------|-----------------------------------|
| SHA3-256         | 32 bytes  | ✅ Yes        | Fast    | High           | Default, general purpose          |
| SHA3-384         | 48 bytes  | ✅ Yes        | Fast    | Very High      | High-security environments        |
| SHA3-512         | 64 bytes  | ✅ Yes        | Fast    | Maximum        | Maximum FIPS-approved strength    |
| PBKDF2-SHA256    | 32 bytes  | ✅ Yes        | Slow    | Very High      | GPU attack resistance             |
| Argon2id         | 32 bytes  | ❌ No         | Slow    | Strongest      | Non-FIPS, maximum security        |

**Performance Comparison** (approx. on modern CPU):
- SHA3-256: ~1 ms per hash
- SHA3-512: ~1.2 ms per hash
- PBKDF2-SHA256 (600k): ~300 ms per hash
- Argon2id (64MB): ~200 ms per hash

**Storage Impact:**
- All algorithms use 16-byte salt (stored per user)
- Hash sizes: 32-64 bytes (stored in KeySlot)
- Total per-user overhead: ~48-80 bytes

---

## User-Facing Documentation

### Preference Dialog Tooltip
```
Select the cryptographic algorithm used to hash usernames in newly created vaults.
Usernames are never stored in plaintext - they are always hashed with a random salt.

• SHA3-256: Recommended for most users (FIPS-approved)
• Argon2id: Strongest security, disables FIPS compliance

This setting only affects NEW vaults. Existing vaults keep their configured algorithm.
```

### Help Documentation Section (for docs/)
```markdown
## Username Hashing Algorithm

KeepTower hashes usernames before storing them in vault files to prevent enumeration
attacks. If an attacker gains access to your vault file, they cannot determine which
users have accounts without trying to authenticate.

**Choosing an Algorithm:**

- **SHA3-256 (Default)**: Fast, FIPS 140-3 approved, suitable for most users
- **SHA3-512**: Stronger FIPS-approved option with larger hash output
- **PBKDF2-SHA256**: Memory-hard, resists GPU cracking (600,000 iterations)
- **Argon2id**: Strongest available, NOT FIPS-approved (disables compliance mode)

**Important Notes:**
1. This setting applies to NEW vaults only - existing vaults retain their algorithm
2. You cannot change an existing vault's algorithm without migration
3. Selecting Argon2id disables FIPS 140-3 compliance mode
4. Enterprise/government users requiring FIPS must use SHA3 or PBKDF2
```

---

## Security Considerations

### FIPS Compliance Enforcement
1. **Build-time Check**: `#ifdef ENABLE_ARGON2` prevents compilation without library
2. **Runtime Check**: `fips_mode_enabled()` hides Argon2id option in FIPS mode
3. **Validation Layer**: `SettingsValidator` enforces constraints even if UI bypassed
4. **Audit Trail**: Log warnings when non-FIPS algorithms selected

### User Education
1. **Visual Indicators**: Warning icon/color for non-FIPS algorithms
2. **Clear Labels**: "FIPS-Approved" vs "NOT FIPS-Approved" in dropdown
3. **Explanatory Text**: Info label explains trade-offs
4. **Documentation**: Link to help docs for detailed explanation

### Defense in Depth
1. **GSettings Schema**: Defines valid enum values
2. **SettingsValidator**: Runtime validation and enforcement
3. **UI Layer**: Prevents invalid selections from being offered
4. **Core Layer**: `VaultOpenHandler` has fallback to SHA3-256 if invalid

---

## Timeline Estimate

**Phase 2: Preferences UI (This Plan)**
- UI Controls: 2-3 hours
- Signal Handlers: 1 hour
- FIPS Enforcement: 1 hour
- Testing: 1-2 hours
- Documentation: 1 hour
- **Total: 6-8 hours (1 day)**

**Optional Phase 3: Advanced Parameters** (can defer)
- PBKDF2 iteration count control
- Argon2 memory/iterations controls
- **Total: 3-4 hours**

---

## Success Criteria

- [ ] User can select username hashing algorithm in Preferences
- [ ] Selected algorithm persists across application restarts
- [ ] New vaults created with selected algorithm
- [ ] Existing vaults unaffected by preference change
- [ ] FIPS mode blocks non-approved algorithms
- [ ] Argon2id hidden when `ENABLE_ARGON2` not defined
- [ ] Clear warnings displayed for non-FIPS choices
- [ ] Info label educates user about algorithm differences
- [ ] All existing tests pass
- [ ] New UI tests validate preference behavior
- [ ] Code adheres to SRP (no god objects)
- [ ] Security considerations documented

---

## References

- GSettings Schema: `data/com.tjdeveng.keeptower.gschema.xml`
- Core Implementation: `src/core/services/UsernameHashService.cc`
- Integration Point: `src/ui/managers/VaultOpenHandler.cc`
- Existing UI: `src/ui/dialogs/PreferencesDialog.cc`
- Security Plan: `docs/developer/USERNAME_HASHING_SECURITY_PLAN.md`
- Vault Spec: `docs/developer/VAULT_FORMAT_V2_SPECIFICATION.md`

---

**Status:** Ready for implementation  
**Next Step:** Create feature branch and implement Step 1 (UI Controls)
