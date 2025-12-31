# Preferences Dialog Refactoring Plan
**Date:** 25 December 2025
**Goal:** Reorganize Security settings into two separate tabs and add vault password history management

## Problem Statement

Current **Security** tab is overcrowded with:
1. Clipboard Protection (account-level)
2. Auto-Lock (vault-level)
3. Password History defaults (NEW vaults only - currently misleading)
4. Undo/Redo (account-level)
5. FIPS-140-3 (vault-level)

**Critical Security Issue:**
- Current "Password History" UI appears to control existing vault behavior
- Actually only sets defaults for NEW vault creation
- When vault opened on different machine, GSettings would incorrectly override vault's stored policy
- **Solution:** Vault policy must ALWAYS come from vault file, never GSettings

## Proposed Structure

### Tab 1: **Appearance** (unchanged)
- Color Scheme

### Tab 2: **Account Security** (NEW)
**Settings that apply to user's local behavior, not vault data:**
- Clipboard Protection
  - Timeout: 5-300 seconds (current: 30s default)
- Undo/Redo
  - Enable/Disable checkbox
  - History limit: 1-100 operations (current: 50 default)
  - Warning message about memory security

### Tab 3: **Vault Security** (NEW - renamed from "Security")
**Settings that apply to vault data and policy:**
- Auto-Lock
  - Enable/Disable checkbox
  - Timeout: 60-3600 seconds (current: 300s default)
- FIPS-140-3 Compliance
  - Enable/Disable checkbox (if available)
  - Status label
  - Restart warning
- **User Password History** (NEW SECTION)
  - Shows CURRENT vault's policy (read-only display)
  - Clear history button (with confirmation)
  - Only visible when vault is open

### Tab 4: **Storage** (unchanged)
- Reed-Solomon Error Correction
- Automatic Backups

## New "User Password History" Section Design

```
┌─────────────────────────────────────────────────────────┐
│ Vault Security                                          │
├─────────────────────────────────────────────────────────┤
│                                                         │
│ Auto-Lock                                               │
│ Lock vault after period of inactivity                   │
│ ☑ Enable auto-lock                                     │
│   Lock after [300] seconds                              │
│                                                         │
│ ─────────────────────────────────────────────────────── │
│                                                         │
│ User Password History                                   │
│ Track previous user passwords to prevent reuse          │
│                                                         │
│ Current vault policy: 5 previous passwords              │
│   (Set when vault was created)                          │
│                                                         │
│ Logged in as: alice                                     │
│ Password history: 3 entries                             │
│                                                         │
│ [Clear My Password History]                             │
│                                                         │
│ ⚠️  This will delete all your password history.        │
│    You will be able to reuse old passwords.            │
│                                                         │
│ ─────────────────────────────────────────────────────── │
│                                                         │
│ FIPS-140-3 Compliance                                   │
│ ...                                                     │
└─────────────────────────────────────────────────────────┘
```

**Admin View:**
- Can clear any user's history
- Shows dropdown to select user
- Confirmation dialog mentions which user

**Regular User View:**
- Can only clear own history
- No user dropdown
- Shows own username and history count

## File Changes Required

### 1. PreferencesDialog.h

**Add Member Variables:**
```cpp
// Account Security page
Gtk::Box m_account_security_box;

// Vault Security page
Gtk::Box m_vault_security_box;

// Vault password history UI (only shown when vault open)
Gtk::Box m_vault_password_history_box;
Gtk::Label m_vault_policy_label;        // "Current vault policy: 5 passwords"
Gtk::Label m_current_user_label;         // "Logged in as: alice"
Gtk::Label m_history_count_label;        // "Password history: 3 entries"
Gtk::Button m_clear_history_button;      // "Clear My Password History"
Gtk::Label m_clear_history_warning;      // Warning about clearing
Gtk::DropDown m_admin_user_dropdown;     // Only for admins
```

**Update Method Declarations:**
```cpp
void setup_account_security_page();  // NEW
void setup_vault_security_page();    // Renamed from setup_security_page()
void update_vault_password_history_ui();  // NEW - refresh when vault changes
void on_clear_password_history_clicked();  // NEW
```

**Remove Member Variables:**
```cpp
Gtk::Box m_security_box;  // Split into account_security and vault_security
Gtk::CheckButton m_password_history_enabled_check;  // Removed (was misleading)
Gtk::Box m_password_history_limit_box;
Gtk::Label m_password_history_limit_label;
Gtk::SpinButton m_password_history_limit_spin;
Gtk::Label m_password_history_limit_suffix;
```

### 2. PreferencesDialog.cc

**Changes in `setup_ui()`:**
```cpp
// Update this line:
setup_security_page();  // Remove

// Add these lines:
setup_account_security_page();  // NEW
setup_vault_security_page();    // NEW
```

**New Method: `setup_account_security_page()`**
- ~150 lines
- Move clipboard protection section
- Move undo/redo section
- Stack add: `m_stack.add(m_account_security_box, "account-security", "Account Security");`

**Updated Method: `setup_vault_security_page()`**
- Rename from `setup_security_page()`
- Keep auto-lock section
- Keep FIPS section
- Add NEW vault password history section
- Stack add: `m_stack.add(m_vault_security_box, "vault-security", "Vault Security");`

**New Method: `update_vault_password_history_ui()`**
```cpp
void PreferencesDialog::update_vault_password_history_ui() {
    // Only show if vault is open
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        m_vault_password_history_box.set_visible(false);
        return;
    }

    // Get vault policy
    auto policy_opt = m_vault_manager->get_vault_security_policy();
    if (!policy_opt) {
        m_vault_password_history_box.set_visible(false);
        return;
    }

    uint32_t depth = policy_opt->password_history_depth;

    // Update policy label
    if (depth == 0) {
        m_vault_policy_label.set_text("Current vault policy: Disabled");
    } else {
        m_vault_policy_label.set_text(
            "Current vault policy: " + std::to_string(depth) +
            (depth == 1 ? " previous password" : " previous passwords")
        );
    }

    // Get current user info
    auto session = m_vault_manager->get_current_user_session();
    if (!session) {
        m_vault_password_history_box.set_visible(false);
        return;
    }

    m_current_user_label.set_text("Logged in as: " + session->username);

    // Get user's password history count
    auto users = m_vault_manager->list_users();
    size_t history_count = 0;
    for (const auto& user : users) {
        if (user.username == session->username) {
            history_count = user.password_history_size;  // Need to add this field!
            break;
        }
    }

    m_history_count_label.set_text(
        "Password history: " + std::to_string(history_count) +
        (history_count == 1 ? " entry" : " entries")
    );

    // Enable button only if there's history to clear
    m_clear_history_button.set_sensitive(history_count > 0);

    m_vault_password_history_box.set_visible(true);
}
```

**New Method: `on_clear_password_history_clicked()`**
```cpp
void PreferencesDialog::on_clear_password_history_clicked() {
    auto session = m_vault_manager->get_current_user_session();
    if (!session) return;

    // Confirmation dialog
    auto* confirm = Gtk::make_managed<Gtk::MessageDialog>(
        *this,
        "Clear password history for " + session->username + "?",
        false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::OK_CANCEL,
        true
    );

    confirm->set_secondary_text(
        "This will delete all stored previous passwords. "
        "You will be able to reuse old passwords after clearing history."
    );

    confirm->signal_response().connect([this, confirm, session](int response) {
        if (response == Gtk::ResponseType::OK) {
            auto result = m_vault_manager->clear_user_password_history(session->username);
            if (result) {
                m_vault_manager->save_vault();
                update_vault_password_history_ui();  // Refresh display

                auto* success = Gtk::make_managed<Gtk::MessageDialog>(
                    *this,
                    "Password history cleared",
                    false,
                    Gtk::MessageType::INFO,
                    Gtk::ButtonsType::OK,
                    true
                );
                success->present();
            } else {
                auto* error = Gtk::make_managed<Gtk::MessageDialog>(
                    *this,
                    "Failed to clear password history",
                    false,
                    Gtk::MessageType::ERROR,
                    Gtk::ButtonsType::OK,
                    true
                );
                error->present();
            }
        }
        confirm->hide();
    });

    confirm->present();
}
```

**Update Constructor:**
- Call `update_vault_password_history_ui()` after `setup_ui()`
- Connect clear button signal:
  ```cpp
  m_clear_history_button.signal_clicked().connect(
      sigc::mem_fun(*this, &PreferencesDialog::on_clear_password_history_clicked));
  ```

**Update `load_settings()`:**
- Remove lines that load password-history-enabled and password-history-limit
- These settings are now ONLY used during vault creation (in MainWindow)

**Update `save_settings()`:**
- Remove lines that save password-history-enabled and password-history-limit
- These are defaults, not runtime preferences

### 3. VaultManager.h

**Add field to UserInfo struct:**
```cpp
struct UserInfo {
    std::string username;
    KeepTower::UserRole role;
    bool must_change_password;
    bool yubikey_enrolled;
    size_t password_history_size;  // NEW - number of entries in history
    uint64_t last_login_time;
    uint64_t password_last_changed;
};
```

### 4. VaultManagerV2.cc

**Update `list_users()` implementation:**
```cpp
// In the loop where UserInfo is populated:
user_info.password_history_size = slot.password_history.size();
```

## Migration Strategy

### Phase 1: Core Structure (30-45 minutes)
1. Update PreferencesDialog.h member variables
2. Add new method declarations
3. Rename `setup_security_page()` to `setup_vault_security_page()`

### Phase 2: Split Security Tab (60-90 minutes)
1. Create `setup_account_security_page()` method
   - Copy clipboard section from old code
   - Copy undo/redo section from old code
   - Add to stack
2. Update `setup_vault_security_page()` method
   - Keep auto-lock section
   - Keep FIPS section
   - Remove password history section (was misleading)
   - Add to stack

### Phase 3: Vault Password History UI (45-60 minutes)
1. Add vault password history section to `setup_vault_security_page()`
2. Implement `update_vault_password_history_ui()`
3. Implement `on_clear_password_history_clicked()`
4. Update VaultManager::UserInfo struct
5. Update VaultManager::list_users() to populate password_history_size

### Phase 4: Testing (30-45 minutes)
1. Build and test all three tabs render correctly
2. Test with vault closed (password history section hidden)
3. Test with vault open (password history section visible)
4. Test clear history button (confirmation and execution)
5. Test as admin (future: dropdown to select user)
6. Test settings persistence

## Potential Issues & Solutions

### Issue 1: Password History Depth Not Settable in UI
**Problem:** No UI to change vault's password_history_depth after creation
**Solution:** This is by design - policy set at vault creation. Document clearly.
**Future:** Could add admin-only "Edit Vault Security Policy" dialog

### Issue 2: GSettings Still Used During Vault Creation
**Status:** This is CORRECT behavior
**Clarification:** GSettings are defaults for NEW vaults, explicitly documented in schema

### Issue 3: Backward Compatibility
**Problem:** Existing vaults opened on new version
**Solution:** No breaking changes - vault format unchanged, just UI reorganization

### Issue 4: Tab Ordering
**Current:** Appearance → Security → Storage
**New:** Appearance → Account Security → Vault Security → Storage
**Impact:** Minimal - users will adapt quickly to new organization

## Testing Checklist

- [ ] Build succeeds without errors
- [ ] All four tabs render correctly
- [ ] Appearance tab unchanged
- [ ] Account Security shows clipboard + undo/redo
- [ ] Vault Security shows auto-lock + FIPS + password history
- [ ] Storage tab unchanged
- [ ] Password history section hidden when no vault open
- [ ] Password history section visible when vault open
- [ ] Policy label shows correct depth
- [ ] History count shows correct number
- [ ] Clear button disabled when history empty
- [ ] Clear button enabled when history has entries
- [ ] Clear button confirmation dialog works
- [ ] Clear button actually clears history
- [ ] Clear button refreshes UI after clearing
- [ ] Non-admin users can only see own history
- [ ] Admin users can select which user to clear (future feature)
- [ ] Settings save/load correctly
- [ ] No GSettings reads during vault open (except at creation)

## Documentation Updates Needed

1. Update PreferencesDialog class documentation
2. Add clear_user_password_history() usage examples
3. Update ROADMAP.md to mark password history complete
4. Create user guide section for password history management

## Estimated Total Time

- Phase 1: 30-45 minutes
- Phase 2: 60-90 minutes
- Phase 3: 45-60 minutes
- Phase 4: 30-45 minutes
- **Total: 2.5-4 hours**

## Notes for Tomorrow

1. Start with Phase 1 (struct changes are quick wins)
2. Test after each phase to catch issues early
3. Consider committing after each phase for safety
4. If time runs short, Phase 3 can be deferred (tabs work without it)
5. Admin user dropdown for clearing others' history is future enhancement

## Questions to Consider Tomorrow

1. Should clear button say "Clear My Password History" or just "Clear History"?
2. Should we show individual history entries with timestamps? (probably not - too detailed)
3. Should admins have a separate admin panel instead of dropdown? (future v0.3 feature)
4. Should we show password history stats in vault info dialog? (nice to have)

---

**Status:** Plan complete, ready for implementation
**Next Session:** Start with Phase 1 header changes
