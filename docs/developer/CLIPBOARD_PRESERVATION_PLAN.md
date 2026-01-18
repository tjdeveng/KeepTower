# Smart Clipboard Preservation - Implementation Plan
**Date**: 2026-01-18
**Status**: Planning Phase
**Previous Attempt**: Jan 16 commit 06bb069 - Rolled back due to segfaults
**Root Cause Identified**: Timeout connection remained active after dialog destruction

---

## Root Cause of Jan 16 Segfault

### What the Jan 16 Commit Did Wrong
The commit tried to preserve clipboard by keeping the timeout connection alive after UserManagementDialog closed:

```cpp
// In dialog close handler:
if (!*password_copied && clipboard_timeout->connected()) {
    clipboard_timeout->disconnect();  // Only disconnect if NOT copied
}
// If copied, timeout stays connected even though dialog is destroyed!
```

**Problem**: The timeout connection lambda captures dialog-related objects (clipboard from dialog). When dialog closes and GTK destroys widgets, the timeout fires later and accesses destroyed objects → **SEGFAULT**.

**Why It Failed**:
1. GTK widget lifecycle: Dialog destroyed when closed
2. Timeout lambda still holds references to clipboard from destroyed dialog
3. Timeout fires → tries to clear clipboard → accesses destroyed GTK widgets
4. Segfault during GTK event processing

**Lesson**: Never keep GTK-related timeouts alive after their parent widget is destroyed.
## Feature Requirements

### User Story
As an **administrator**, when I create a standard user account with a temporary password:
1. I copy the temporary password to clipboard
2. I close the vault (clipboard should NOT be cleared)
3. I communicate the password to the user (via email, Slack, etc.)
4. User logs in with the pasted password
5. **Then** clipboard can be cleared

**Current Problem**: Clipboard is cleared immediately when vault closes, making it impossible to paste the temp password for the new user.

---

## Architecture Analysis

### Current Clipboard Flow (Working)
```
ClipboardManager
  ├── copy_text() → starts auto-clear timer (30s default)
  ├── clear_immediately() → clears clipboard now
  └── set_clear_timeout_seconds() → configures timeout

MainWindow
  ├── on_close_vault() → calls clipboard_manager->clear_immediately()
  └── on_lock_vault() → calls clipboard_manager->clear_immediately()

UserManagementDialog
  └── "Copy Password" button → calls clipboard_manager->copy_text(password)
```

**Key Observation**: ClipboardManager is a controller owned by MainWindow. It has NO knowledge of:
- What content was copied (password vs other data)
- Where the copy came from (UserManagementDialog vs PasswordDetailsWidget)
- User workflow context (admin creating user vs user copying own password)

### Previous Failed Approach (Violated Widget Lifecycle)
```
❌ BAD APPROACH ❌

UserManagementDialog
  └── signal_response() lambda
       ├── Captures: clipboard_timeout (shared_ptr<sigc::connection>)
       ├── Captures: password_copied (shared_ptr<bool>)
       ├── On copy: clipboard_timeout = clipboard.set_timeout(30s)
       └── On close: if password_copied, DON'T disconnect timeout
           └── Timeout lives beyond dialog lifetime
               └── Lambda accesses destroyed dialog's clipboard
                   └── SEGFAULT when timeout fires
```

**Problems**:
1. Timeout connection outlives dialog (GTK widget lifecycle violation)
2. Lambda captures clipboard from dialog that gets destroyed
3. Shared pointer doesn't help - it keeps connection alive but can't prevent widget destruction
4. Timeout fires after dialog destroyed → accesses freed memory → segfault

---

## Proper Architecture (Respects GTK Widget Lifecycle)

### Core Principle: Clipboard Must Outlive Dialog
**The fundamental issue**: UserManagementDialog's clipboard is tied to the dialog widget. When dialog closes, clipboard reference becomes invalid.

**Solution**: Use MainWindow's clipboard (lives for entire application lifetime), not dialog's clipboard.

### Key Insight
- **ClipboardManager** already exists and is owned by MainWindow
- It has all the timeout management we need
- It's designed specifically for this use case
- It lives for the entire application lifetime (no destruction issues)

### Current ClipboardManager (Already Works!)
```cpp
class ClipboardManager {
    void copy_text(const std::string& text);  // Copies + starts auto-clear timer
    void clear_immediately();                  // Clears clipboard now
    void set_clear_timeout_seconds(int seconds);  // Configure timeout

    sigc::signal<void()> signal_cleared();    // Notification when cleared

private:
    Glib::RefPtr<Gdk::Clipboard> m_clipboard;  // Lives with MainWindow
    sigc::connection m_clear_timeout;
    int m_timeout_seconds;
};
```

**This already does exactly what we need!** We just need to use it correctly.

---

## The Simple Solution

### What We Need
1. UserManagementDialog copies temp password using **MainWindow's clipboard** (not dialog's)
2. Clipboard auto-clear timer is already managed by ClipboardManager
3. Just **don't clear** when dialog closes if password was copied
4. Let the existing timer do its job

### Implementation (Minimal Changes)

**Option 1: Pass ClipboardManager to Dialog** (Recommended)
```cpp
// UserAccountHandler.cc - already has clipboard_manager reference
void UserAccountHandler::on_manage_users() {
    auto dialog = new UserManagementDialog(m_window, m_vault_manager,
                                           m_clipboard_manager);  // Pass it!
    // ... existing code ...
}

// UserManagementDialog.h
class UserManagementDialog {
    UserManagementDialog(Gtk::Window& parent,
                        VaultManager* vault_manager,
                        ClipboardManager* clipboard_manager);  // Add parameter

private:
    ClipboardManager* m_clipboard_manager;  // Store reference
};

// UserManagementDialog.cc - temp password copy
void show_temporary_password_copy_button() {
    copy_button->signal_clicked().connect([this, temp_password]() {
        // Use MainWindow's clipboard, not dialog's!
        if (m_clipboard_manager) {
            m_clipboard_manager->copy_text(temp_password);
            // ClipboardManager starts auto-clear timer (30s default)
            // Timer lives in MainWindow scope - safe!
        }
    });
}

// UserManagementDialog.cc - dialog close
dialog->signal_response().connect([dialog](int response) {
    // Do NOT clear clipboard on close
    // Let ClipboardManager's timer handle it
    dialog->hide();
    delete dialog;
});
```

**That's it!** No shared pointers, no timeout management, no GTK lifecycle issues.
**Idea**: ClipboardManager knows what TYPE of content was copied.

```cpp
enum class ClipboardContext {
    STANDARD,           // Normal copy (auto-clear on vault close)
    TEMP_PASSWORD,      // Temp password (preserve on vault close)
    USER_WORKFLOW       // User explicitly requested preservation
};

class ClipboardManager {
    void copy_text(const std::string& text, ClipboardContext context = ClipboardContext::STANDARD);
    void clear_immediately(bool force = false);  // force=true ignores preservation

private:
    ClipboardContext m_current_context;
    bool m_preserve_until_login;
};
```

**Flow**:
1. UserManagementDialog copies temp password:
   ```cpp
   clipboard_manager->copy_text(password, ClipboardContext::TEMP_PASSWORD);
   ```

2. MainWindow closes vault:
   ```cpp
   clipboard_manager->clear_immediately();  // Checks context, skips if TEMP_PASSWORD
   ```

3. V2AuthenticationHandler completes login:
   ```cpp
   clipboard_manager->clear_immediately(force=true);  // Always clears
   ```

**Pros**:
- ✅ ClipboardManager owns all clipboard logic (SRP)
- ✅ No god object
- ✅ Simple API
- ✅ No timing issues

**Cons**:
- ⚠️ ClipboardManager needs to know about user workflows (weak SRP)
- ⚠️ Hardcoded contexts (not extensible)

---

### Option B: Signal-Based Preservation
**Idea**: UserManagementDialog signals intent, ClipboardManager responds.

```cpp
class ClipboardManager {
    sigc::signal<void()> signal_about_to_clear();

    void copy_text(const std::string& text);
    void clear_immediately();
    void defer_clear();  // Called by signal handler to prevent clear

private:
    bool m_clear_deferred;
};

class UserManagementDialog {
    void on_temp_password_copied() {
        // Signal to any interested parties
        m_clipboard_manager->signal_about_to_clear().connect([this]() {
            m_clipboard_manager->defer_clear();
        });
    }
};
```

**Pros**:
- ✅ Loose coupling via signals
- ✅ Extensible (any component can defer clearing)
- ✅ ClipboardManager doesn't know about workflows

**Cons**:
- ❌ Complex signal lifecycle management
- ❌ Who disconnects the signal?
- ❌ Potential memory leaks (signal handlers outliving dialogs)
- ❌ Hard to debug

---

### Option C: Explicit Preservation Token
**Idea**: Caller gets a token that prevents clearing until returned.

```cpp
class ClipboardPreservationToken {
    // RAII token - clipboard won't clear while this exists
    ~ClipboardPreservationToken() { m_manager->release_preservation(); }
private:
    ClipboardManager* m_manager;
};

class ClipboardManager {
    [[nodiscard]] std::unique_ptr<ClipboardPreservationToken> preserve();
    void clear_immediately();

private:
    int m_preservation_count;  // Reference counting
};

class UserManagementDialog {
    std::unique_ptr<ClipboardPreservationToken> m_preservation_token;

    void on_copy_temp_password() {
        m_clipboard_manager->copy_text(password);
        m_preservation_token = m_clipboard_manager->preserve();
        // Token lives until dialog closes or user logs in
    }
};
```

**Pros**:
- ✅ RAII lifetime management
- ✅ Clear ownership (whoever holds token controls preservation)
- ✅ No timing issues
- ✅ Type-safe

**Cons**:
- ⚠️ Who owns the token? (UserManagementDialog? MainWindow? V2AuthenticationHandler?)
- ⚠️ Token transfer complexity
- ⚠️ What if dialog closes before login?

---

### Option D: State Machine in ClipboardManager
**Idea**: ClipboardManager has explicit states for different workflows.

```cpp
enum class ClipboardState {
    IDLE,                    // No content
    STANDARD_CONTENT,        // Normal content (auto-clear on vault close)
    PRESERVED_CONTENT,       // Content preserved (skip clear on vault close)
    FORCE_CLEAR              // Clear on next opportunity
};

class ClipboardManager {
    void copy_text(const std::string& text);
    void enable_preservation();   // Called by UserManagementDialog
    void disable_preservation();  // Called by V2AuthenticationHandler
    void clear_immediately();     // Respects state

private:
    ClipboardState m_state;
};
```

**Flow**:
1. UserManagementDialog: Copy temp password
   ```cpp
   clipboard_manager->enable_preservation();
   clipboard_manager->copy_text(password);
   ```

2. MainWindow: Close vault
   ```cpp
   clipboard_manager->clear_immediately();  // Checks state, skips if PRESERVED
   ```

3. V2AuthenticationHandler: Login succeeds
   ```cpp
   clipboard_manager->disable_preservation();
   clipboard_manager->clear_immediately();  // Now clears
   ```

**Pros**:
- ✅ Explicit state management
- ✅ ClipboardManager owns all logic
- ✅ Simple enable/disable API
- ✅ Easy to test

**Cons**:
- ⚠️ State transitions must be carefully managed
- ⚠️ Who is responsible for disable_preservation()?

---

## Recommended Approach

### **Hybrid: Option A + Option D**
Combine context-aware copying with state machine for robustness.

```cpp
class ClipboardManager {
public:
    enum class CopyContext {
        NORMAL,          // Regular clipboard content
        SENSITIVE,       // Password (auto-clear on timeout)
        TEMP_PASSWORD    // Temporary password (preserve on vault close)
    };

    void copy_text(const std::string& text, CopyContext context = CopyContext::NORMAL);
    void clear_immediately();
    void force_clear();  // Clears regardless of context

    // State queries
    bool is_preservation_active() const { return m_preservation_active; }

private:
    CopyContext m_current_context;
    bool m_preservation_active;
    sigc::connection m_preservation_timeout;

    void handle_clear_request();  // Smart clearing based on context
};
```

**Implementation Details**:

1. **UserManagementDialog** (temp password copy):
   ```cpp
   void UserManagementDialog::on_copy_temp_password() {
       m_clipboard_manager->copy_text(temp_password,
                                       ClipboardManager::CopyContext::TEMP_PASSWORD);
       // ClipboardManager automatically sets preservation mode
   }
   ```

2. **MainWindow** (vault close):
   ```cpp
   void MainWindow::on_close_vault() {
       // ... existing close logic ...

       // Smart clear: respects preservation context
       m_clipboard_manager->clear_immediately();
   }
   ```

3. **V2AuthenticationHandler** (login success):
   ```cpp
   void V2AuthenticationHandler::on_authentication_complete() {
       // ... existing auth logic ...

       // Force clear regardless of context
       if (m_clipboard_manager) {
           m_clipboard_manager->force_clear();
       }
   }
   ```

4. **ClipboardManager** (smart clearing logic):
   ```cpp
   void ClipboardManager::clear_immediately() {
       if (m_current_context == CopyContext::TEMP_PASSWORD && m_preservation_active) {
           Log::info("Skipping clear - temp password preservation active");
           return;
       }
       do_clear();
   }

   void ClipboardManager::force_clear() {
       m_preservation_active = false;
       do_clear();
   }
   ```

**Responsibility Assignment**:
- **ClipboardManager**: Owns preservation logic, knows about contexts
- **UserManagementDialog**: Declares intent (TEMP_PASSWORD context)
- **MainWindow**: Requests clear (respects preservation)
- **V2AuthenticationHandler**: Forces clear (overrides preservation)

---

## Implementation Steps

### Phase 1: Pass ClipboardManager to UserManagementDialog
**Files**: `UserAccountHandler.cc`, `UserManagementDialog.h`, `UserManagementDialog.cc`

1. Add `ClipboardManager*` parameter to UserManagementDialog constructor
2. Store as member variable `m_clipboard_manager`
3. Pass from UserAccountHandler (already has reference)

**Code Changes**:
```cpp
// UserManagementDialog.h
ClipboardManager* m_clipboard_manager{nullptr};

// UserManagementDialog constructor
UserManagementDialog(..., ClipboardManager* clipboard_manager)
    : ..., m_clipboard_manager(clipboard_manager) {}

// UserAccountHandler.cc
auto dialog = new UserManagementDialog(m_window, m_vault_manager, m_clipboard_manager);
```

### Phase 2: Use ClipboardManager for Temp Password Copy
**Files**: `UserManagementDialog.cc`

1. Find the temp password "Copy to Clipboard" button handler
2. Replace `dialog->get_clipboard()->set_text()` with `m_clipboard_manager->copy_text()`
3. Remove ALL timeout management code from dialog
4. Remove shared_ptr<sigc::connection> and password_copied tracking

**Before (Broken)**:
```cpp
auto clipboard_timeout = std::make_shared<sigc::connection>();
auto password_copied = std::make_shared<bool>(false);

copy_button.signal_clicked().connect([clipboard_timeout, password_copied]() {
    auto clipboard = dialog->get_clipboard();
    clipboard->set_text(password);
    *password_copied = true;
    *clipboard_timeout = Glib::signal_timeout().connect(...);
});

dialog->signal_response().connect([clipboard_timeout, password_copied]() {
    if (!*password_copied && clipboard_timeout->connected()) {
        clipboard_timeout->disconnect();  // Broken!
    }
});
```

**After (Simple & Correct)**:
```cpp
copy_button.signal_clicked().connect([this, temp_password]() {
    if (m_clipboard_manager) {
        m_clipboard_manager->copy_text(temp_password);
        // Done! ClipboardManager handles timeout automatically
    }
});

dialog->signal_response().connect([dialog]() {
    // Just close, don't mess with clipboard
    dialog->hide();
    delete dialog;
});
```

### Phase 3: Don't Clear on Dialog Close
**Files**: `UserManagementDialog.cc`

Remove any `clipboard->set_text("")` or clipboard clearing code from dialog close handlers.

### Phase 4: Testing
1. Admin creates user → copies temp password
2. Dialog closes immediately
3. Clipboard should still have password
4. Wait 30 seconds → clipboard cleared automatically
5. No segfaults!

**That's the entire implementation** - about 10 lines of actual changes.

---

## Testing Checklist

### Unit Tests
- [ ] ClipboardManager context switching
- [ ] Preservation respects timeout
- [ ] force_clear() overrides preservation
- [ ] clear_immediately() respects context

### Integration Tests
- [ ] Admin creates user → copies temp password → closes vault → clipboard preserved
- [ ] User logs in → clipboard cleared
- [ ] Timeout expires (60s) → clipboard cleared
- [ ] Vault locked → clipboard cleared (force)
- [ ] Multiple copy operations → latest context wins

### Manual Testing
- [ ] Full admin→user workflow
- [ ] YubiKey enrollment doesn't interfere
- [ ] Password change dialog doesn't crash
- [ ] No segfaults during GTK rendering
- [ ] Clipboard actually cleared after login

---

## Success Criteria

### Functional
- ✅ Temp password preserved when vault closes
- ✅ Clipboard cleared after successful login
- ✅ Safety timeout prevents indefinite preservation
- ✅ All 42 unit tests still pass

### Architectural
- ✅ No god objects
- ✅ SRP maintained (each component has one responsibility)
- ✅ Loose coupling via clear API boundaries
- ✅ No GTK lifecycle interference

### Code Quality
- ✅ Well-documented public APIs
- ✅ Clear responsibility assignment
- ✅ Minimal changes to existing code
- ✅ No regressions in existing features

---

## Rollback Plan

If implementation fails:
```bash
# Saved patch location
/tmp/clipboard_preservation_rollback_20260118_081036.patch

# Quick revert
git checkout -- src/ui/controllers/ClipboardManager.*
git checkout -- src/ui/dialogs/UserManagementDialog.*
git checkout -- src/ui/managers/V2AuthenticationHandler.*
git checkout -- src/ui/windows/MainWindow.*

# Verify tests pass
meson test -C build
```

---

## Open Questions

1. **Timeout Duration**: 60 seconds enough? Configurable?
2. **UI Feedback**: Should we show "Clipboard preserved for user creation" message?
3. **Multiple Users**: What if admin creates 2 users in a row?
4. **Cancellation**: Should admin be able to cancel preservation manually?
5. **Persistence**: Should preservation survive app restart? (NO - security risk)

---

## Next Steps

1. **Review this plan** with team/maintainer
2. **Get approval** for Option A+D hybrid approach
3. **Implement Phase 1** (ClipboardManager only)
4. **Test Phase 1** in isolation
5. **Proceed phase by phase** with testing at each step
6. **No commits until fully tested** and verified stable

---

## References

- Previous attempt: `/tmp/clipboard_preservation_rollback_20260118_081036.patch`
- Debug session: `DEBUG_SESSION_2026-01-17.md`
- Test results: All 42 tests passing (baseline)
- Code structure: Phase 5 refactoring (controller pattern, SRP)
