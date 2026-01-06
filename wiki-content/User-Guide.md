# User Guide

Complete reference for all KeepTower features.

## Table of Contents

- [Vault Management](#vault-management)
- [Multi-User Vaults (V2)](#multi-user-vaults-v2)
- [Account Management](#account-management)
- [Password Operations](#password-operations)
- [Search and Organization](#search-and-organization)
- [Preferences](#preferences)
- [Backups and Recovery](#backups-and-recovery)
- [Keyboard Shortcuts](#keyboard-shortcuts)

---

## Vault Management

### Creating a New Vault

**Menu:** File → New Vault
**Shortcut:** `Ctrl+N`

1. Choose location and filename
2. Select vault format:
   - **V1 (Legacy):** Single-user, backwards compatible
   - **V2 (Recommended):** Multi-user with enhanced security
3. For V2 vaults, configure:
   - Admin username and password
   - Security policy (password requirements, history depth)
   - Optional YubiKey authentication
4. Vault is created and opened

**File Formats:**
- `.vault` - Encrypted binary format with optional Reed-Solomon encoding
- V1: Single-user, AES-256-GCM
- V2: Multi-user, role-based access, password history

### Opening a Vault

**Menu:** File → Open Vault
**Shortcut:** `Ctrl+O`

1. Select `.vault` file
2. Enter master password
3. Click Open

**Note:** Opening a vault closes any currently open vault (with save prompt if needed).

### Saving Changes

**Menu:** File → Save
**Shortcut:** `Ctrl+S`

Changes are saved to the vault file. If backups are enabled, a backup is created first.

**Auto-save:** Changes are automatically saved when:
- Closing the vault
- Exiting the application
- Opening a different vault

### Closing a Vault

**Menu:** File → Close Vault
**Shortcut:** `Ctrl+W`

Closes the current vault. If there are unsaved changes, you'll be prompted to save.

### Vault Properties

View vault information:
- File path
- File size
- Last modified date
- Number of accounts
- Vault format (V1/V2)
- FEC status (enabled/disabled)
- FEC redundancy percentage

---

## Multi-User Vaults (V2)

### Overview

V2 vaults support multiple users with role-based access control:

**User Roles:**
- **Admin:** Full access, can manage users, change security policy
- **Standard:** Can view/edit accounts, cannot manage users or policy

### Creating a V2 Vault

1. **File → New Vault**
2. Choose **V2 (Multi-User)** format
3. Configure admin account:
   - Username (cannot be changed later)
   - Strong password
4. Set security policy:
   - Minimum password length (8-128 characters)
   - Password history depth (0-24 previous passwords)
   - YubiKey requirement (optional)
5. Click **Create**

### User Management

**Menu:** Vault → User Management (Admin only)

#### Adding Users

1. Click **Add User**
2. Enter username
3. Set initial password (user must change on first login)
4. Select role: Admin or Standard
5. Click **Create**

#### Changing User Passwords

**Self-service:**
1. **Vault → Change Password**
2. Enter current password
3. Enter new password (must meet security policy)
4. Confirm new password

**Admin reset:**
1. **Vault → User Management**
2. Select user
3. Click **Reset Password**
4. User must change password on next login

#### Removing Users

**Admin only:**
1. **Vault → User Management**
2. Select user
3. Click **Remove User**
4. Confirm removal

**Note:** Cannot remove yourself or the last admin

### Password History

V2 vaults track password history to prevent reuse:

- **Configurable Depth:** 0-24 previous passwords
- **Per-User:** Each user has their own password history
- **Admin Control:** History depth set in vault security policy
- **Validation:** System rejects passwords matching history

**Admin can clear history:**
1. **Vault → User Management**
2. Select user
3. Click **Clear Password History**

### Security Policy

**Admin only:** Define vault-wide security requirements

**Settings:**
- **Min Password Length:** 8-128 characters
- **Password History Depth:** 0-24 entries
- **Require YubiKey:** Optional hardware authentication
- **PBKDF2 Iterations:** Key derivation work factor

**Changing Policy:**
1. **Vault → Security Policy** (Admin only)
2. Adjust settings
3. Click **Apply**
4. All users affected on next password change

### YubiKey Authentication (Optional)

**Requirements:**
- YubiKey 5 series with FIDO2 hmac-secret support
- HMAC-SHA256 (FIPS-140-3 compliant)
- YubiKey PIN configured

**Setup:**
1. Insert YubiKey during vault creation
2. Enable "Require YubiKey" in security policy
3. Enter YubiKey PIN when prompted
3. Program challenge-response slot
4. All users must use same YubiKey model/programming

**Login with YubiKey:**
1. Enter username and password
2. Insert YubiKey when prompted
3. Touch YubiKey button
4. Vault unlocks

### Migrating V1 to V2

**Menu:** Vault → Convert to V2

1. Open V1 vault
2. Select **Vault → Convert to V2**
3. Create admin credentials
4. Configure security policy
5. Original vault backed up automatically
6. New V2 vault created

**Note:** One-way conversion. Keep V1 backup until confident.

---

## Account Management

### Adding an Account

**Button:** + (Add Account)
**Shortcut:** `Ctrl+A`

**Required Fields:**
- Account Name
- Password

**Optional Fields:**
- Username/Email
- URL/Website
- Notes (future feature)

**Tips:**
- Use descriptive names: "Gmail Personal", "Work Email", "GitHub"
- Include the URL for easy reference
- Use the password generator for new accounts

### Editing an Account

1. Select account from list
2. Click **Edit** button
3. Modify fields
4. Click **Save**

Changes take effect immediately.

### Deleting an Account

1. Select account from list
2. Click **Delete** button
3. Confirm deletion

**⚠️ Warning:** Deletion is permanent (unless restored from backup).

### Viewing Account Details

Select an account from the list to view:
- Account name
- Username
- Password (hidden by default)
- URL
- Created date
- Last modified date

---

## Password Operations

### Showing/Hiding Passwords

Click the **eye icon** (Show/Hide) to toggle password visibility.

**Security:** Passwords are hidden by default. They remain hidden when switching between accounts.

### Copying Passwords

**Button:** Copy Password
**Shortcut:** `Ctrl+C` (when account selected)

Copies the password to clipboard.

**Auto-clear:** Clipboard is automatically cleared after 45 seconds for security.

### Copying Usernames

Click **Copy Username** to copy the username/email to clipboard.

### Opening URLs

If an account has a URL, click the **🔗 Open** button to open it in your default browser.

### Password Generator (Current Behavior)

When creating or editing an account, you can generate a random password:

**Settings:**
- Length: 12-32 characters
- Includes: uppercase, lowercase, numbers, symbols
- Excludes: ambiguous characters (0/O, 1/l)

**Future Enhancement:** More customization options planned.

---

## Search and Organization

### Searching Accounts

Type in the search bar to filter accounts in real-time.

**Searches:**
- Account names
- Usernames
- URLs

**Tips:**
- Search is case-insensitive
- Partial matches work (e.g., "git" finds "GitHub", "GitLab")
- Clear search to show all accounts

### Sorting (Current Implementation)

Accounts are displayed in the order they were added.

**Future Enhancement:** Sorting by name, date, or usage planned.

---

## Preferences

Access via **Edit** → **Preferences** or `Ctrl+,`

### Reed-Solomon Error Correction

**Purpose:** Protects vault data from corruption (bit rot, bad storage sectors)

**Settings:**

- **Enable Reed-Solomon FEC**
  - ☑️ Checked: Error correction enabled
  - ☐ Unchecked: Disabled (smaller file size)

- **Redundancy Percentage:** 5-50%
  - **5%:** Minimal protection, smallest overhead
  - **10%:** Recommended balance
  - **25%:** High protection for critical data
  - **50%:** Maximum protection

**File Size Impact:**
- 10% redundancy ≈ 10% larger file
- 25% redundancy ≈ 25% larger file
- Protection increases with redundancy

**Apply Settings:**

- **☐ Apply to current vault** (unchecked)
  - Changes only affect **new vaults** created in the future
  - Current open vault keeps its original settings

- **☑️ Apply to current vault** (checked)
  - Changes apply to the **currently open vault**
  - Does NOT change defaults for new vaults
  - Vault must be saved to persist changes

**Recommendation:** Enable with 10-25% redundancy for important vaults.

### Backup Settings

**Enable Automatic Backups**
- ☑️ Checked: Creates backups before each save
- ☐ Unchecked: No automatic backups

**Number of Backups to Keep:** 1-10
- Older backups are automatically deleted
- Backups stored as: `vault.vault.backup.YYYYMMDD_HHMMSS_mmm`

**Location:** Same directory as the vault file

### Appearance

**Color Scheme:**
- **System Default:** Follow desktop theme (light/dark)
- **Light:** Always use light theme
- **Dark:** Always use dark theme

Theme changes take effect immediately.

---

## Backups and Recovery

### Automatic Backups

When enabled, KeepTower creates a timestamped backup before saving:

```
passwords.vault                          ← Current vault
passwords.vault.backup.20251208_143052_123  ← Backup 1
passwords.vault.backup.20251208_120033_456  ← Backup 2
```

### Restoring from Backup

1. Close KeepTower
2. Locate backup files in the same directory as your vault
3. Rename backup to remove `.backup.TIMESTAMP` suffix
4. Open the restored file in KeepTower

**Example:**
```bash
# Backup your current vault first
cp passwords.vault passwords.vault.current

# Restore from backup
cp passwords.vault.backup.20251208_143052_123 passwords.vault
```

### Manual Backups

In addition to automatic backups, you should periodically:

1. Copy vault file to external storage
2. Test that the backup opens successfully
3. Store in secure location (encrypted drive recommended)

---

## Keyboard Shortcuts

### File Operations
- `Ctrl+N` - New Vault
- `Ctrl+O` - Open Vault
- `Ctrl+S` - Save Vault
- `Ctrl+W` - Close Vault
- `Ctrl+Q` - Quit KeepTower

### Account Operations
- `Ctrl+A` - Add Account
- `Ctrl+E` - Edit Selected Account
- `Delete` - Delete Selected Account
- `Ctrl+C` - Copy Password (when account selected)
- `Ctrl+F` - Focus Search Bar

### Application
- `Ctrl+,` - Open Preferences
- `Ctrl+?` - Help (future)
- `F1` - About

### Navigation
- `Up/Down Arrows` - Navigate account list
- `Tab` - Move between fields in dialogs
- `Enter` - Confirm dialogs
- `Escape` - Cancel/Close dialogs

---

## Tips and Tricks

### Organizing Large Vaults

**Use naming conventions:**
- Prefix by category: `[Work] Email`, `[Personal] Banking`
- Include service name: `Gmail - Personal`, `Gmail - Work`
- Add keywords for search: `Amazon (Shopping)`, `AWS (Cloud)`

### Password Best Practices

1. **Unique passwords** - Never reuse passwords
2. **Strong passwords** - Use generator for 16+ character passwords
3. **Regular updates** - Change passwords periodically (especially after breaches)
4. **Two-factor authentication** - Enable 2FA where available (TOTP support coming)

### Vault Organization

**Multiple vaults for different purposes:**
- Personal accounts
- Work accounts
- Shared family accounts
- Archived/old accounts

**Security vs. Convenience:**
- More vaults = better isolation
- Single vault = easier to manage

### Performance

- Vaults with 1000+ accounts may see slower load times
- Search performance remains fast regardless of vault size
- Consider splitting very large vaults

---

## Troubleshooting

### "Cannot open vault - incorrect password"

- Double-check master password (case-sensitive)
- Try typing slowly to avoid typos
- Check if Caps Lock is on
- **No recovery available** if password is truly forgotten

### "Vault file is corrupted"

1. Try opening a backup file
2. If FEC is enabled, try again (may auto-correct)
3. Report issue on GitHub with error details

### "Permission denied"

- Check file permissions: `chmod 600 vault.vault`
- Ensure you own the file: `chown $USER vault.vault`
- Move vault to your home directory if on network drive

### Application crashes

1. Check terminal output for errors
2. Try deleting config: `rm -rf ~/.config/keeptower/`
3. Report issue with logs on GitHub

---

## Advanced Topics

### File Format Details

See **[[Security]]** for technical details about:
- Encryption algorithm (AES-256-GCM)
- Key derivation (PBKDF2)
- Reed-Solomon encoding
- File structure

### Command Line Usage (Future)

Command-line interface is planned for automation and scripting.

### API/Plugin System (Future)

Extension API for third-party integrations planned for v0.4.x+.

---

## Getting Help

- **Documentation:** This wiki
- **Issues:** [GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)
- **Discussions:** [GitHub Discussions](https://github.com/tjdeveng/KeepTower/discussions)
- **Email:** Check repository for contact info

---

**Next:** Learn about security in **[[Security]]** →
