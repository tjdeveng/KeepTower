# User Guide

Complete reference for all KeepTower features.

## Table of Contents

- [Vault Management](#vault-management)
- [Multi-User Vaults (V2)](#multi-user-vaults-v2)
- [Account Management](#account-management)
- [Password Operations](#password-operations)
- [Import and Export](#import-and-export)
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
2. Configure vault settings:
   - Admin username and password
   - Security policy (password requirements, history depth)
   - Optional YubiKey authentication
3. Vault is created and opened

**File Format:**
- `.vault` - Encrypted binary format with optional Reed-Solomon encoding
- Multi-user with role-based access, password history, and AES-256-GCM encryption

### Opening a Vault

**Menu:** File → Open Vault
**Shortcut:** `Ctrl+O`

1. Select `.vault` file
2. Enter master password
3. Touch YubiKey if 2FA is enabled
4. Click Open

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
- FEC status (enabled/disabled)
- FEC redundancy percentage
- YubiKey 2FA status

---


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
- YubiKey with HMAC-SHA1 challenge-response
- Must be programmed during vault creation

**Setup:**
1. Insert YubiKey during vault creation
2. Enable "Require YubiKey" in security policy
3. Program challenge-response slot
4. All users must use same YubiKey model/programming

**Login with YubiKey:**
1. Enter username and password
2. Insert YubiKey when prompted
3. Touch YubiKey button
4. Vault unlocks

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
- Notes

**Tips:**
- Use descriptive names: "Gmail Personal", "Work Email", "GitHub"
- Include the URL for easy reference
- Use the password generator for new accounts

### Editing an Account

1. Select account from list
2. Click **Edit** button
3. Modify any fields
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
- Notes
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

**Auto-clear:** Clipboard is automatically cleared after 30 seconds for security.

### Copying Usernames

Click **Copy Username** to copy the username/email to clipboard.

### Opening URLs

If an account has a URL, click the **🔗 Open** button to open it in your default browser.

### Password Generator

When creating or editing an account, you can generate a random password:

**Settings:**
- Length: 12-32 characters
- Includes: uppercase, lowercase, numbers, symbols
- Excludes: ambiguous characters (0/O, 1/l)

**Best Practice:** Use 16+ character passwords for maximum security.

---

## Import and Export

**⚠️ Security Warning:** Exported files contain your passwords in **unencrypted plaintext**. Handle with care!

### Exporting Accounts

**Menu:** File → Export Accounts
**Shortcut:** `Ctrl+E`

Export your vault to CSV, KeePass XML, or 1Password 1PIF format.

**Steps:**
1. Click File → Export Accounts
2. Read and accept the security warning
3. Re-authenticate with your master password (and YubiKey if enabled)
4. Choose export format and location
5. File is created with restricted permissions (0600)

**Supported Formats:**

- **CSV** - Comma-separated values (universal compatibility)
  - Fields: Account Name, Username, Password, URL, Notes
  - RFC 4180 compliant
  - Fully tested and verified

- **KeePass 2.x XML** - Compatible with KeePass password manager
  - Standard KeePass 2.x XML format
  - Round-trip tested within KeepTower
  - Import into KeePass for cross-platform access

- **1Password 1PIF** - Compatible with 1Password
  - 1Password Interchange Format (JSON-based)
  - Round-trip tested within KeepTower
  - Useful for migration to/from 1Password

**Security Measures:**
- Password re-authentication required
- Files created with 0600 permissions (owner only)
- fsync() called to ensure data integrity
- Warning dialog about plaintext risks

**Best Practices:**
- Delete exported file after use
- Store on encrypted storage if keeping
- Never share exported files
- Use secure file transfer if needed

### Importing Accounts

**Menu:** File → Import Accounts
**Shortcut:** `Ctrl+I`

Import accounts from CSV, KeePass XML, or 1Password 1PIF files.

**Steps:**
1. Click File → Import Accounts
2. Select file to import
3. KeepTower detects format automatically
4. Accounts are imported into current vault
5. Save vault to persist changes

**Supported Formats:**

- **CSV** - Auto-detected by `.csv` extension
  - Expected columns: Account Name, Username, Password, URL, Notes
  - Header row required
  - Handles quoted fields and escaped commas

- **KeePass XML** - Auto-detected by `.xml` extension
  - Standard KeePass 2.x XML format
  - Imports Title, UserName, Password, URL, Notes
  - Round-trip compatible

- **1Password 1PIF** - Auto-detected by `.1pif` extension
  - JSON-based format
  - Imports title, username, password, location, notes
  - Round-trip compatible

**Import Behavior:**
- New accounts are added to existing vault
- Duplicate detection not implemented (will create duplicates)
- Invalid entries are skipped with error reporting
- Partial imports supported (continues on errors)
- 100MB file size limit for security

**After Import:**
- Review imported accounts
- Delete duplicate entries if any
- Save vault to persist changes
- Securely delete import file

### Format Compatibility

| Format | Export | Import | Round-trip Tested | External App Tested |
|--------|--------|--------|-------------------|---------------------|
| CSV | ✅ | ✅ | ✅ | ✅ |
| KeePass XML | ✅ | ✅ | ✅ | ⚠️ Pending |
| 1Password 1PIF | ✅ | ✅ | ✅ | ⚠️ Pending |

**Round-trip tested:** Export from KeepTower → Import back into KeepTower → Data verified
**External app tested:** Export from KeepTower → Import into KeePass/1Password → Verified

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

### Organizing Large Vaults

**Use naming conventions:**
- Prefix by category: `[Work] Email`, `[Personal] Banking`
- Include service name: `Gmail - Personal`, `Gmail - Work`
- Add keywords for search: `Amazon (Shopping)`, `AWS (Cloud)`

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

### YubiKey Settings

**Hardware 2FA:**
- Configure YubiKey challenge-response authentication
- Add backup YubiKeys for redundancy
- View serial numbers of authorized keys
- Remove keys from authorized list

**Benefits:**
- Two-factor vault encryption
- Protection against password compromise
- Hardware-based security

### Backup Settings

**Enable Automatic Backups**
- ☑️ Checked: Creates backups before each save
- ☐ Unchecked: No automatic backups

**Number of Backups to Keep:** 1-50
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
passwords.vault                             ← Current vault
passwords.vault.backup.20251213_143052_123  ← Backup 1
passwords.vault.backup.20251213_120033_456  ← Backup 2
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
cp passwords.vault.backup.20251213_143052_123 passwords.vault
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
- `Ctrl+I` - Import Accounts
- `Ctrl+E` - Export Accounts

### Account Operations
- `Ctrl+A` - Add Account
- `Ctrl+E` - Edit Selected Account
- `Delete` - Delete Selected Account
- `Ctrl+C` - Copy Password (when account selected)
- `Ctrl+F` - Focus Search Bar

### Application
- `Ctrl+,` - Open Preferences
- `F1` - About

### Navigation
- `Up/Down Arrows` - Navigate account list
- `Tab` - Move between fields in dialogs
- `Enter` - Confirm dialogs
- `Escape` - Cancel/Close dialogs

---

## Tips and Tricks

### Password Best Practices

1. **Unique passwords** - Never reuse passwords
2. **Strong passwords** - Use generator for 16+ character passwords
3. **Regular updates** - Change passwords periodically (especially after breaches)
4. **Two-factor authentication** - Enable 2FA where available
5. **YubiKey 2FA** - Use hardware 2FA for KeepTower vault

### Vault Organization

**Multiple vaults for different purposes:**
- Personal accounts
- Work accounts
- Shared family accounts
- Archived/old accounts

**Security vs. Convenience:**
- More vaults = better isolation
- Single vault = easier to manage

### Migration Strategies

**From other password managers:**
1. Export from current manager (CSV usually supported)
2. Import into KeepTower
3. Review and clean up entries
4. Enable YubiKey 2FA for added security
5. Configure backups and FEC

**To other password managers:**
1. Export from KeepTower (choose appropriate format)
2. Import into target manager
3. Verify all accounts transferred
4. Securely delete export file

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
- Ensure YubiKey is inserted if 2FA is enabled
- **No recovery available** if password is truly forgotten

### "Vault file is corrupted"

1. Try opening a backup file
2. If FEC is enabled, try again (may auto-correct)
3. Report issue on GitHub with error details

### "Permission denied"

- Check file permissions: `chmod 600 vault.vault`
- Ensure you own the file: `chown $USER vault.vault`
- Move vault to your home directory if on network drive

### Import/Export Issues

- **Export fails:** Ensure disk space available
- **Import fails:** Check file format and size (<100MB)
- **Format not detected:** Rename file with correct extension (.csv, .xml, .1pif)
- **Partial import:** Review error messages, fix invalid entries

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
- YubiKey integration

### Command Line Usage

KeepTower currently requires the GUI. Command-line interface is planned for future versions.

---

## Getting Help

- **Documentation:** This wiki
- **Issues:** [GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)
- **Discussions:** [GitHub Discussions](https://github.com/tjdeveng/KeepTower/discussions)

---

**Next:** Learn about security in **[[Security]]** →
