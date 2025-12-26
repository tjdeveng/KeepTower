# Security Best Practices

## Overview

KeepTower is designed with security as a top priority, but proper configuration and usage are essential to maintain the security of your password vaults. This document outlines important security considerations and best practices.

## Vault Creation Security

### Critical: Verify Settings Before Creating Vaults

**Important Security Note:** Default settings for new vaults are stored in your user preferences and can be modified through the Preferences dialog. When creating a new vault, especially for high-security purposes, always verify and set appropriate security settings **before** or **immediately after** vault creation.

#### Default Settings That Affect New Vaults

The following settings are used as defaults when creating new vaults:

1. **Vault Security Settings:**
   - **Clipboard Timeout** - How long passwords stay in clipboard (default: 30 seconds)
   - **Auto-Lock Timeout** - Inactivity period before vault locks (default: 300 seconds)
   - **Undo/Redo** - Whether operations can be undone (default: enabled)
   - **Undo History Limit** - Maximum operations kept in memory (default: 50)
   - **Account Password History** - Prevent reusing account entry passwords (default: 5 passwords)
   - **Vault User Password History** - Prevent reusing vault authentication passwords (default: 5 passwords)

2. **Storage Settings:**
   - **Reed-Solomon Error Correction** - File corruption protection (default: disabled)
   - **Redundancy Level** - Error correction strength (default: 10%)
   - **Automatic Backups** - Timestamped backup creation (default: enabled)
   - **Backup Count** - Number of backups to maintain (default: 5)

### Recommended Security Practice

**For high-security vaults:**

1. **Before creating the vault:**
   - Open Preferences (File → Preferences)
   - Review all security settings in Account Security and Vault Security tabs
   - Set appropriate defaults for your security requirements
   - Click Apply to save settings

2. **When creating the vault:**
   - Use a strong master password
   - For V2 vaults: Consider enabling YubiKey requirement if available
   - Set appropriate PBKDF2 iterations (higher = more secure but slower)

3. **After creating the vault:**
   - Open the vault
   - Open Preferences again
   - Verify that the vault's security settings match your requirements
   - Adjust if necessary (settings will be saved to the vault file)

### Why This Matters

- **Vault-Specific Settings:** Security settings (clipboard timeout, auto-lock, undo/redo, password history) are stored IN the vault file and travel with it
- **Shared System Risk:** If multiple users have access to your system account, they could modify default settings
- **No Retroactive Protection:** Changing defaults doesn't affect existing vaults - each vault maintains its own settings

## Multi-User Environments

### Shared Computer Scenarios

If you share a computer or user account with others:

1. **Always verify settings before creating sensitive vaults**
2. **Check vault settings after opening existing vaults** - they may have different policies than expected
3. **Consider using separate system user accounts** for different security contexts
4. **Use V2 multi-user vaults** if multiple people need access to the same vault (each user has their own authentication)

### V2 Multi-User Vault Security

For V2 vaults with multiple users:

- **Administrator Role:** Has full control including user management and security policy
- **Standard User Role:** Can view/edit accounts but cannot change security policy
- **Security Policy:** Set once at vault creation, enforced for all users:
  - Minimum password length
  - Password history depth
  - YubiKey requirement
  - PBKDF2 iterations

## Password Security

### Master Password Guidelines

Your master password (vault authentication password) is critical:

1. **Length:** Minimum 12 characters, 16+ recommended for high-security vaults
2. **Complexity:** Mix of uppercase, lowercase, numbers, and symbols
3. **Uniqueness:** Never reuse passwords from other services
4. **Password Managers:** You can use another password manager to store your KeepTower master password (though this creates a dependency)

### Password History

**Account Password History** (Account Security tab):
- Prevents reusing passwords when updating stored account entries (e.g., Gmail, GitHub accounts)
- Stored per account within the vault
- Default: Remember 5 previous passwords per account

**Vault User Password History** (Vault Security tab):
- Prevents reusing passwords when users change their vault authentication passwords
- Stored per user in V2 vaults
- Default: Remember 5 previous passwords per user
- **V2 only:** Not applicable to V1 vaults

## Storage Security

### Reed-Solomon Error Correction

- **Purpose:** Protects vault files from bit rot and storage corruption
- **Trade-off:** Increases file size
- **Recommendation:** Enable for vaults stored on:
  - Unreliable media (USB drives, SD cards)
  - Aging hard drives
  - Cloud storage with potential corruption
- **Not needed for:** Modern SSDs with error correction, well-maintained systems

### Backup Strategy

1. **Enable automatic backups** (default: enabled)
2. **Set appropriate backup count** (default: 5 backups)
3. **Store backups securely** - they contain the same sensitive data as the vault
4. **Regular offsite backups** - Copy vault files to separate physical location
5. **Test backup restoration** periodically

### File System Security

- **Store vaults on encrypted partitions** (LUKS, BitLocker, FileVault)
- **Set appropriate file permissions:** `chmod 600 vault.vault` (owner read/write only)
- **Avoid network shares** unless using encrypted network protocols
- **USB storage:** Always encrypt the drive containing vaults

## Session Security

### Auto-Lock Configuration

- **Enable auto-lock** (default: enabled)
- **Set appropriate timeout** for your environment:
  - High-security: 60-180 seconds
  - Standard: 300 seconds (5 minutes)
  - Low-risk: 600-900 seconds (10-15 minutes)
- **Never disable** auto-lock in multi-user environments

### Clipboard Security

- **Clipboard timeout** (default: 30 seconds)
- Automatically clears copied passwords after timeout
- **Recommendation:** 10-30 seconds for most environments
- **Consideration:** Shorter timeout = more secure but less convenient

### Undo/Redo Security Trade-off

**When to disable undo/redo:**
- High-security environments where password exposure in memory is a concern
- Undo history keeps passwords in memory until cleared
- Disabling prevents undo but avoids memory retention

**When to keep enabled:**
- Standard use cases where convenience outweighs memory exposure risk
- Allows recovery from accidental deletions or modifications

## FIPS 140-3 Compliance

If operating in a FIPS-required environment:

1. **Enable FIPS mode** in Preferences → Vault Security
2. **Requires:** OpenSSL 3.5+ with FIPS provider properly configured
3. **Restart application** after enabling
4. **Note:** KeepTower uses FIPS-validated cryptographic modules but is not itself FIPS-certified

## Audit and Monitoring

### Regular Security Reviews

1. **Periodically review vault settings:** Open Preferences with vault open
2. **Check password history:** Vault Security tab shows current policy
3. **Verify auto-lock is working:** Test by waiting for timeout period
4. **Review backup files:** Ensure they're properly secured

### Security Indicators

- **Lock icon:** Indicates vault is locked
- **Session label:** Shows current user and role (V2 vaults)
- **Preferences dialog:** Shows per-vault vs. global settings

## Incident Response

### If Settings Are Compromised

If you suspect default settings have been modified maliciously:

1. **Immediately verify settings** before creating new vaults
2. **Check existing vaults:** Open each vault and verify its security settings in Preferences
3. **Consider re-creating vaults** with verified settings if compromise is severe
4. **Change passwords** if unauthorized access is suspected

### If Vault Is Compromised

1. **Change all passwords** stored in the compromised vault
2. **Enable 2FA** on affected accounts where available
3. **Review account activity logs** on services stored in the vault
4. **Create new vault** with verified security settings
5. **Migrate accounts** to new vault
6. **Securely delete** compromised vault file (multiple overwrites or full disk encryption)

## Future Improvements

The development team is considering these security enhancements:

- **Vault Management Application:** Separate tool for vault creation with enforced security policies
- **Security Policy Templates:** Pre-configured settings for different security levels
- **Administrator-Locked Settings:** Prevent modification of critical settings by non-admin users
- **Vault Signing:** Cryptographic signatures to detect tampering

These features would address the current limitation where any user with system access can modify default settings.

## Getting Help

For security questions or to report vulnerabilities:

- **GitHub Issues:** https://github.com/yourusername/KeepTower/issues
- **Security Email:** security@keeptower.example.com (if established)
- **Documentation:** https://github.com/yourusername/KeepTower/docs

## Summary Checklist

Before creating a high-security vault:

- [ ] Review and set appropriate default settings in Preferences
- [ ] Choose a strong master password (16+ characters)
- [ ] Enable Reed-Solomon if storing on unreliable media
- [ ] Enable automatic backups
- [ ] Set appropriate auto-lock timeout (≤ 300 seconds)
- [ ] Set appropriate clipboard timeout (≤ 30 seconds)
- [ ] Consider password history depth (5-12 passwords recommended)
- [ ] After creation, verify vault settings match expectations
- [ ] Store vault file on encrypted storage
- [ ] Set restrictive file permissions (chmod 600)
- [ ] Create offsite backup copy

**Remember:** Security settings are stored in each vault file and persist across systems. Take time to configure them properly at creation!
