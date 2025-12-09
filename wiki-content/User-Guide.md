# User Guide

Complete reference for all KeepTower features.

## Table of Contents

- [Vault Management](#vault-management)
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
2. Create master password
3. Confirm master password
4. Vault is created and opened

**File Format:** `.vault` - Encrypted binary format with optional Reed-Solomon encoding

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
- FEC status (enabled/disabled)
- FEC redundancy percentage

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
