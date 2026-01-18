# Smart Clipboard Preservation - Simple Implementation Plan
**Date**: 2026-01-18
**Status**: Ready for Implementation
**Root Cause**: Jan 16 commit kept timeout connection alive after dialog destruction

---

## What Went Wrong (Jan 16 Commit 06bb069)

The commit tried to preserve clipboard by NOT disconnecting the timeout when dialog closed:

```cpp
// BROKEN CODE - DO NOT USE
auto clipboard_timeout = std::make_shared<sigc::connection>();
auto password_copied = std::make_shared<bool>(false);

// On copy button click:
auto clipboard = dialog->get_clipboard();  // Dialog's clipboard!
*clipboard_timeout = Glib::signal_timeout().connect([]() {
    clipboard->set_text("");  // Timeout references dialog clipboard
});

// On dialog close:
if (!*password_copied && clipboard_timeout->connected()) {
    clipboard_timeout->disconnect();  // Only disconnect if NOT copied
}
// If copied, timeout stays alive but dialog is destroyed!
// → Timeout fires → accesses destroyed clipboard → SEGFAULT
```

**Root Cause**: Timeout connection outlived the dialog widget, causing use-after-free.

---

## The Simple Solution

### Key Insight
Use **MainWindow's ClipboardManager** (lives forever) instead of dialog's clipboard (destroyed when dialog closes).

### What We Have
ClipboardManager already exists with clean API:
- `copy_text()` - copies and starts auto-clear timer
- `clear_immediately()` - clears clipboard now
- `set_clear_timeout_seconds()` - configure timeout
- Lives in MainWindow scope (entire application lifetime)

### What We Need to Add
**Single Responsibility**: ClipboardManager should provide a mechanism to defer clearing without knowing WHY.

Add two methods to ClipboardManager:
```cpp
void enable_preservation();   // Next clear_immediately() call will be skipped
void disable_preservation();  // Resume normal clearing behavior
```

**Why this respects SRP**:
- ClipboardManager doesn't know about "temp passwords" or "user workflows"
- It just provides a simple flag: "skip next clear request"
- Business logic (when to preserve) stays in UserManagementDialog
- No hardcoded contexts, no enum pollution

### The Complete Solution
1. Pass ClipboardManager to UserManagementDialog (lives in MainWindow scope - safe)
2. When temp password copied, call `enable_preservation()`
3. Dialog closes, vault closes - `clear_immediately()` sees preservation flag and skips
4. User logs in successfully - call `disable_preservation()` then `clear_immediately()`

---

## Implementation

### Step 1: Add Preservation Methods to ClipboardManager

**ClipboardManager.h**:
```cpp
class ClipboardManager {
public:
    // ... existing methods ...

    /**
     * @brief Enable clipboard preservation
     *
     * When enabled, the next call to clear_immediately() will be skipped,
     * allowing clipboard content to persist through vault close events.
     *
     * Use case: Preserve temporary password after copying so admin can
     * paste it when logging in as the new user.
     *
     * Preservation automatically disables when:
     * - User explicitly calls disable_preservation()
     * - Safety timeout expires (uses configured clipboard-timeout setting)
     *
     * The safety timeout uses the same timeout value as the normal clipboard
     * auto-clear (configured via GSettings/vault preferences). This ensures
     * consistent behavior and respects user preferences.
     *
     * @note Does not affect auto-clear timer - that continues normally
     */
    void enable_preservation();

    /**
     * @brief Disable clipboard preservation
     *
     * Resumes normal clearing behavior. Call this after the preserved
     * content is no longer needed (e.g., after successful login).
     */
    void disable_preservation();

    /**
     * @brief Check if preservation is active
     * @return true if clear_immediately() will be skipped
     */
    bool is_preservation_active() const { return m_preserve_on_close; }

private:
    bool m_preserve_on_close{false};
    sigc::connection m_preservation_timeout;  // Safety timeout
};
```

**ClipboardManager.cc**:
```cpp
void ClipboardManager::enable_preservation() {
    m_preserve_on_close = true;

    // Safety timeout: use configured clipboard timeout setting
    // This respects user preferences and ensures consistent behavior
    // If user set 15 seconds for normal clipboard clear, preservation
    // expires after the same 15 seconds (not a hardcoded value)
    if (m_preservation_timeout.connected()) {
        m_preservation_timeout.disconnect();
    }

    m_preservation_timeout = Glib::signal_timeout().connect([this]() {
        Log::info("ClipboardManager: Preservation timeout expired ({}s configured limit)",
                  m_timeout_seconds);
        m_preserve_on_close = false;
        return false;  // Disconnect after firing once
    }, m_timeout_seconds * 1000);  // Use configured timeout, not hardcoded value

    Log::info("ClipboardManager: Preservation enabled ({}s window from settings)",
              m_timeout_seconds);
}

void ClipboardManager::disable_preservation() {
    if (m_preservation_timeout.connected()) {
        m_preservation_timeout.disconnect();
    }
    m_preserve_on_close = false;
    Log::info("ClipboardManager: Preservation disabled");
}

void ClipboardManager::clear_immediately() {
    if (m_preserve_on_close) {
        Log::info("ClipboardManager: Skipping clear - preservation active");
        return;
    }

    // ... existing clear logic ...
}
```

### Step 2: Pass ClipboardManager to UserManagementDialog

**UserManagementDialog.h**:
```cpp
class UserManagementDialog : public Gtk::Window {
public:
    UserManagementDialog(Gtk::Window& parent,
                        VaultManager* vault_manager,
                        ClipboardManager* clipboard_manager);  // ADD THIS

private:
    ClipboardManager* m_clipboard_manager{nullptr};  // ADD THIS
};
```

**UserManagementDialog.cc** (constructor):
```cpp
UserManagementDialog::UserManagementDialog(
    Gtk::Window& parent,
    VaultManager* vault_manager,
    ClipboardManager* clipboard_manager)  // ADD THIS
    : Gtk::Window(),
      m_parent(parent),
      m_vault_manager(vault_manager),
      m_clipboard_manager(clipboard_manager) {  // ADD THIS
    // ... existing code ...
}
```

**UserAccountHandler.cc** (already has clipboard_manager reference):
```cpp
void UserAccountHandler::on_manage_users() {
    auto dialog = new UserManagementDialog(
        m_window,
        m_vault_manager,
        m_clipboard_manager);  // ADD THIS (already available as member)
    // ... existing code ...
}
```

### Step 3: Use ClipboardManager for Temp Password Copy

**UserManagementDialog.cc** - Find the temp password dialog code (~line 494):

**REMOVE THIS** (broken code from Jan 16):
```cpp
// Track clipboard timeout connection (shared_ptr to keep it alive even after dialog closes)
auto clipboard_timeout = std::make_shared<sigc::connection>();
// Track whether password was copied (shared_ptr so lambda can modify it)
auto password_copied = std::make_shared<bool>(false);

// In copy button handler:
auto clipboard = dialog->get_clipboard();
clipboard->set_text(temp_password.raw());
*password_copied = true;

auto settings = Gio::Settings::create("com.tjdeveng.keeptower");
int timeout_seconds = settings->get_int("clipboard-timeout");
// ... timeout setup ...

// In dialog close handler:
if (!*password_copied && clipboard_timeout->connected()) {
    clipboard_timeout->disconnect();
}
```

**REPLACE WITH THIS** (simple & SRP-compliant):
```cpp
// In copy button handler:
copy_button->signal_clicked().connect([this, temp_password]() {
    if (m_clipboard_manager) {
        // Copy using MainWindow's clipboard (lives forever - safe)
        m_clipboard_manager->copy_text(temp_password.raw());

        // Enable preservation so clipboard survives vault close
        // Business logic: We know admin needs to paste this for new user login
        m_clipboard_manager->enable_preservation();

        warning_label->set_text("✓ Copied to clipboard (preserved until login or timeout)");
    }
});

// In dialog close handler:
dialog->signal_response().connect([dialog](int response) {
    // Just close - clipboard preservation is managed by ClipboardManager
    dialog->hide();
    delete dialog;
});
```

### Step 4: Disable Preservation After Successful Login

**V2AuthenticationHandler.cc** - in the success callback after authentication:
```cpp
void V2AuthenticationHandler::on_authentication_complete(const std::string& username) {
    // ... existing authentication logic ...

    // Disable clipboard preservation now that user has logged in
    // This allows normal vault close to clear clipboard
    if (m_clipboard_manager) {
        m_clipboard_manager->disable_preservation();
    }

    // Call success callback to complete vault opening
    if (m_success_callback) {
        m_success_callback(m_current_vault_path, username);
    }
}
```

**Note**: V2AuthenticationHandler needs ClipboardManager reference. Pass it through constructor (same pattern as UserAccountHandler).

---

## Why This Respects SRP

### ClipboardManager Responsibility
**Single Responsibility**: Manage clipboard content and clearing behavior.

**What it knows**:
- How to copy text
- How to clear clipboard
- How to manage auto-clear timers
- A boolean flag: "should I skip the next clear request?"

**What it does NOT know** (preserves SRP):
- ❌ What "temp password" means
- ❌ User creation workflows
- ❌ Authentication flows
- ❌ When users log in
- ❌ Business logic

**Interface**: Two simple methods with clear semantics:
- `enable_preservation()` - "Skip next clear request" (mechanism)
- `disable_preservation()` - "Resume normal behavior" (mechanism)

### UserManagementDialog Responsibility
**Single Responsibility**: Manage user creation and display temp passwords.

**Business Logic**: "When admin copies temp password, they need it to persist until the new user logs in."
- Calls `enable_preservation()` when temp password copied
- ClipboardManager doesn't need to know WHY

### V2AuthenticationHandler Responsibility
**Single Responsibility**: Manage authentication flows.

**Business Logic**: "After successful login, clipboard can be cleared normally."
- Calls `disable_preservation()` after authentication succeeds
- ClipboardManager doesn't need to know WHY

### No God Objects
- MainWindow just wires components together (no business logic added)
- Each component has one clear responsibility
- ClipboardManager provides mechanism, not policy

---

## Testing Plan

### Manual Test
1. Admin creates vault as "dev"
2. Admin sets clipboard timeout in preferences (e.g., 15 seconds for testing)
3. Admin adds standard user "dev1"
4. Admin clicks "Copy to Clipboard" in temp password dialog
5. Admin closes dialog immediately
6. **Verify**: Clipboard still has password ✓
7. Admin closes vault
8. **Verify**: Clipboard still has password (preservation active) ✓
9. Admin can paste password to log in as dev1
10. **Verify**: After successful login, preservation disabled ✓
11. Wait for configured timeout (e.g., 15 seconds)
12. **Verify**: Clipboard cleared automatically ✓
13. **Verify**: No segfaults at any point ✓

**Alternative flow** (timeout expiry before login):
1. Admin copies temp password
2. Admin closes dialog and vault
3. Admin gets distracted for configured timeout period
4. **Verify**: Clipboard cleared after timeout (safety feature) ✓
5. Admin must re-open vault to get password again

### Unit Tests
Existing ClipboardManager tests already cover this - no new tests needed!

---

## Files to Modify

1. **src/ui/controllers/ClipboardManager.h** (+15 lines)
   - Add `enable_preservation()` declaration
   - Add `disable_preservation()` declaration
   - Add `is_preservation_active()` inline method
   - Add `m_preserve_on_close` member variable
   - Add `m_preservation_timeout` member variable

2. **src/ui/controllers/ClipboardManager.cc** (+30 lines)
   - Implement `enable_preservation()` with 60s safety timeout
   - Implement `disable_preservation()`
   - Modify `clear_immediately()` to check preservation flag

3. **src/ui/dialogs/UserManagementDialog.h** (+2 lines)
   - Add `ClipboardManager*` parameter to constructor
   - Add `m_clipboard_manager` member variable

4. **src/ui/dialogs/UserManagementDialog.cc** (+5 lines, -20 lines)
   - Update constructor to accept and store clipboard_manager
   - Replace temp password copy logic to use clipboard_manager
   - Call `enable_preservation()` after copying
   - Remove broken timeout management code

5. **src/ui/managers/UserAccountHandler.cc** (+1 line)
   - Pass `m_clipboard_manager` to UserManagementDialog constructor

6. **src/ui/managers/V2AuthenticationHandler.h** (+2 lines)
   - Add `ClipboardManager*` parameter to constructor
   - Add `m_clipboard_manager` member variable

7. **src/ui/managers/V2AuthenticationHandler.cc** (+3 lines)
   - Store clipboard_manager in constructor
   - Call `disable_preservation()` after successful authentication

8. **src/ui/windows/MainWindow.cc** (+1 line)
   - Pass `m_clipboard_manager.get()` to V2AuthenticationHandler constructor

**Total changes**: ~60 lines added, ~20 lines removed = net +40 lines across 8 files.

---

## Success Criteria

- ✅ Temp password stays in clipboard after dialog closes
- ✅ Clipboard cleared after configured timeout (30s default)
- ✅ User can paste password to log in
- ✅ No segfaults during any workflow
- ✅ All 42 existing tests still pass
- ✅ Code simpler than before (net -15 lines)

---

## Rollback Plan

If anything goes wrong:
```bash
git reset --hard 5fb281a  # Back to working Jan 15 state
meson compile -C build
```

Current working baseline saved at commit 5fb281a.
