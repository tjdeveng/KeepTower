# FAQ - Frequently Asked Questions

Common questions about KeepTower answered.

## General Questions

### What is KeepTower?

KeepTower is a password manager for Linux that stores your passwords in an encrypted vault on your local machine. It uses military-grade encryption (AES-256-GCM) and includes optional Reed-Solomon error correction to protect against data corruption.

### Why another password manager?

KeepTower fills a specific niche:
- **Linux-native:** Built with GTK4/libadwaita for deep Linux integration
- **Error correction:** Reed-Solomon FEC protects against bit rot
- **Hardware 2FA:** YubiKey challenge-response authentication
- **Multi-format import/export:** CSV, KeePass XML, 1Password 1PIF
- **Local-first:** No cloud dependency, your data stays on your machine
- **Modern:** Uses latest GTK4 and C++23
- **Open source:** GPL-3.0 licensed, auditable code

### Is KeepTower ready for production use?

**Current Status:** v0.2.5-beta

- ✅ **Encryption:** Production-ready (AES-256-GCM, PBKDF2)
- ✅ **Core features:** Stable and tested (12/12 tests passing)
- ✅ **YubiKey 2FA:** Hardware security implemented
- ✅ **Import/Export:** Multi-format support (CSV, KeePass, 1Password)
- ⚠️ **UI/Features:** Beta stage, polish ongoing

**Recommendation:** Suitable for personal use and testing. All critical security features are implemented and the code is open source for community review.

### Is it free?

Yes! KeepTower is **free and open source** software licensed under GPL-3.0-or-later. No subscriptions, no ads, no premium tiers.

---

## Security Questions

### How secure is KeepTower?

KeepTower uses industry-standard encryption:
- **AES-256-GCM:** Same encryption used by governments and militaries
- **PBKDF2:** 100,000 iterations for key derivation
- **YubiKey 2FA:** Optional hardware-based second factor
- **Memory protection:** Sensitive data locked in RAM with secure clearing
- **Thread safety:** Mutex protection for vault operations
- **Open source:** Code available for security review

See **[[Security]]** for detailed information.

### What if I forget my master password?

**There is no password recovery.** This is by design - a recovery mechanism would be a security backdoor.

**Recommendations:**
- Write down your master password and store in a safe
- Use a strong but memorable passphrase
- Consider using YubiKey 2FA for additional protection
- Keep backup YubiKeys if using hardware 2FA

### Can someone crack my vault with a supercomputer?

**Short answer:** Not with a strong master password.

**Long answer:**
- AES-256 is computationally infeasible to crack by brute force
- PBKDF2 with 100,000 iterations makes password guessing expensive
- YubiKey 2FA adds hardware-based protection
- **Weak password = weak security** regardless of encryption
- Strong 16+ character password + YubiKey = effectively uncrackable with current technology

### Is my vault safe from bit rot?

**With Reed-Solomon enabled: Yes!**

Reed-Solomon error correction can automatically repair corruption:
- 10% redundancy: Can correct minor corruption
- 25% redundancy: Can correct significant corruption
- 50% redundancy: Can recover from extensive damage

**Without FEC:** Corruption may make vault unopenable.

**Recommendation:** Enable FEC with 10-25% redundancy + keep backups.

### Has KeepTower been security reviewed?

**Yes**, through multiple internal reviews:
- Uses well-established cryptography (OpenSSL)
- Open source code available for community review
- Based on industry best practices (OWASP, NIST)
- Multiple security-focused code reviews completed
- All tests passing with clean valgrind runs
- See CODE_REVIEW.md for detailed security analysis

**External professional audit:** Not planned at this time due to resource constraints. The code is open source and available for independent security review by qualified professionals.

---
- All tests passing with clean valgrind runs

---

## Usage Questions

### Can I use KeepTower on multiple computers?

**Yes**, with manual sync:

1. Store vault on USB drive or cloud storage (Dropbox, etc.)
2. Open vault on any computer with KeepTower installed
3. Save changes
4. Changes persist in the vault file

**Caution with cloud storage:**
- Vault is encrypted, but cloud provider has the file
- Consider encrypting the cloud folder too (cryptomator, etc.)
- Beware of sync conflicts if editing on multiple devices simultaneously

**Future:** Native sync support is planned for v0.4.x+

### Can I import passwords from another password manager?

**Yes!** (as of v0.2.5-beta)

**Supported import formats:**
- **CSV** - Universal format supported by most password managers
- **KeePass 2.x XML** - Import from KeePass/KeePassXC
- **1Password 1PIF** - Import from 1Password

**How to import:**
1. Export from your current password manager
2. Click File → Import Accounts in KeepTower
3. Select the exported file
4. Format is auto-detected from file extension
5. Review imported accounts
6. Save vault
7. **Securely delete the exported file!**

See **[[User Guide]]** for detailed instructions.

### Can I export my passwords?

**Yes!** (as of v0.2.5-beta)

**Supported export formats:**
- **CSV** - Universal format (tested and verified)
- **KeePass 2.x XML** - For use with KeePass/KeePassXC (round-trip tested)
- **1Password 1PIF** - For use with 1Password (round-trip tested)

**Security measures:**
- Password re-authentication required
- YubiKey touch required if 2FA enabled
- Files created with 0600 permissions (owner only)
- Security warning dialog

**⚠️ Warning:** Exported files contain **unencrypted plaintext** passwords. Delete after use!

See **[[User Guide]]** for detailed instructions.

### Can I use KeepTower on my phone?

**No.** Mobile apps are not currently planned.

KeepTower is focused on providing a secure, feature-rich password manager for Linux desktop environments. Mobile support would require significant development resources and is not part of the current roadmap.

**Current workaround:**
- Use a mobile password manager for on-the-go access
- Sync vault file via cloud storage and access on Linux desktop
- Many users maintain separate mobile password managers
---

## YubiKey Questions

### What is YubiKey 2FA and why use it?

YubiKey 2FA adds hardware-based security to your vault:
- **Two-factor encryption:** Password + YubiKey both required
- **Protection against password compromise:** Vault stays secure even if password is stolen
- **Hardware-based:** Cannot be copied or phished
- **Offline:** No internet required, works anywhere

### What YubiKey models are supported?

Any YubiKey with **HMAC-SHA1 challenge-response** support:
- YubiKey 5 Series (all variants)
- YubiKey 4 Series
- Security Key Series
- NEO

**Configuration slot:** Uses slot 2 (programmable)

### Can I use multiple YubiKeys?

**Yes!** (as of v0.2.4-beta)

Configure backup YubiKeys for redundancy:
1. Open Preferences
2. YubiKey Settings
3. Add additional keys
4. Each key is authorized independently

**Recommendation:** Keep a backup YubiKey in a safe place!

### What if I lose my YubiKey?

**With backup keys configured:**
- Use any authorized backup YubiKey
- No data loss

**Without backup keys:**
- **Vault is inaccessible**
- No recovery mechanism (by design for security)
- This is why backup keys are strongly recommended!

---

## Import/Export Questions

### What formats can I import/export?

| Format | Import | Export | Tested |
|--------|--------|--------|--------|
| CSV | ✅ | ✅ | ✅ Verified |
| KeePass 2.x XML | ✅ | ✅ | ✅ Round-trip |
| 1Password 1PIF | ✅ | ✅ | ✅ Round-trip |

**Round-trip tested:** Export → Import → Data verified intact

### Will export create duplicates if I import back?

**Yes.** Duplicate detection is not currently implemented.

**Workaround:**
- Don't import into the same vault you exported from
- Use export for migration or backup purposes
- Manually review and delete duplicates if needed

**Future:** Duplicate detection planned for v0.3.x

### Can I export to an encrypted format?

**Not yet.** All exports are plaintext.

**Security recommendations:**
- Delete exported file immediately after use
- Store on encrypted storage if keeping
- Never share exported files
- Use secure file transfer if needed

**Future:** Encrypted export option planned for v0.3.x

### How do I migrate from another password manager?

**Example: From KeePass to KeepTower**

1. Open your vault in KeePass
2. File → Export → KeePass XML
3. Open KeepTower, create new vault
4. File → Import Accounts
5. Select the XML file
6. Review imported accounts
7. Save vault
8. **Securely delete the XML file**
9. Configure YubiKey 2FA in KeepTower
10. Enable Reed-Solomon FEC

**Example: From 1Password to KeepTower**

1. Export from 1Password as 1PIF format
2. Follow same steps as above
3. Delete exported 1PIF file

---

## Technical Questions

### What file format does KeepTower use?

**Custom encrypted format:**
- Binary format (not text/XML)
- Structure: Salt + IV + Flags + Encrypted(Protobuf) + Auth Tag
- Optionally Reed-Solomon encoded
- Optional YubiKey response integration

**Why custom format?**
- Allows Reed-Solomon error correction
- Authenticated encryption (GCM)
- YubiKey 2FA integration
- Smaller file size (binary vs XML)

**Import/export** to standard formats supported via CSV, KeePass XML, 1Password 1PIF.

### Can I access my vault from the command line?

**Not yet.** CLI interface is planned for a future release.

**Use cases:**
- Scripting and automation
- Headless servers
- Quick password lookups

### What are the system requirements?

**Minimum:**
- Linux kernel 5.x+
- GTK4 (4.10+)
- 100 MB RAM
- 10 MB disk space

**Recommended:**
- Modern Linux distribution (Ubuntu 24.04+, Fedora 39+)
- GTK4 4.14+
- 256 MB RAM
- SSD storage

**Supported architectures:**
- x86_64 (primary)
- aarch64 (ARM64) - Should work but less tested

### Does KeepTower collect any data?

**No.** KeepTower collects zero data:
- No telemetry
- No analytics
- No crash reports
- No network connections (except opening URLs you click)

Your data stays on your machine. Period.

### Why is the file size larger with Reed-Solomon?

Reed-Solomon adds redundant data for error correction:
- 10% redundancy = ~10% larger file
- 25% redundancy = ~25% larger file
- Trade-off: Size vs. protection against corruption

**Example:**
- Original vault: 100 KB
- With 10% FEC: 110 KB
- With 25% FEC: 125 KB

### Can I disable Reed-Solomon after enabling it?

**Yes**, but with caution:

1. Open vault with FEC enabled
2. Go to Preferences
3. Check "Apply to current vault"
4. Uncheck "Enable Reed-Solomon"
5. Save preferences
6. Save vault

**Warning:** Once disabled, vault is no longer protected against corruption. You can re-enable it later.

---

## Troubleshooting

### KeepTower won't open my vault

**Possible causes:**

1. **Wrong password**
   - Double-check (case-sensitive)
   - Caps Lock off?
   - Try typing slowly

2. **YubiKey not inserted**
   - Insert YubiKey if 2FA enabled
   - Touch key when LED flashes
   - Try backup YubiKey if primary lost

3. **Corrupted vault file**
   - Try opening a backup
   - Check file size (corrupted = unusual size)
   - If FEC enabled, try again (may auto-repair)

4. **Wrong file**
   - Ensure it's actually a `.vault` file
   - Check creation date

### "Cannot load vault" error

Try these steps:

1. Check file permissions: `ls -l vault.vault`
2. Ensure you own the file: `chown $USER vault.vault`
3. Check available disk space: `df -h`
4. Try opening a backup
5. Report issue on GitHub with error details

### Import/export errors

**Import fails:**
- Check file size (<100MB limit)
- Verify file format (correct extension)
- Check for file corruption
- Review error messages for specific issues

**Export fails:**
- Check disk space
- Verify write permissions
- Try different location
- Check vault is properly unlocked

### Application crashes on startup

**Possible fixes:**

1. Delete config: `rm -rf ~/.config/keeptower/`
2. Update GTK4: `sudo dnf update gtkmm4.0`
3. Check for library conflicts
4. Run from terminal to see errors: `./keeptower`
5. Report issue with error output

### Backup files are taking up space

**Normal behavior** if backups enabled.

**To manage:**
- Reduce "Number of backups to keep" in Preferences
- Manually delete old backups (`.backup.TIMESTAMP` files)
- Disable backups if you have external backup solution

**Don't delete all backups** - keep at least a few recent ones!

---

## Feature Requests

### Can KeepTower support TOTP/2FA codes?

**Planned for v0.3.x!**

This is a high-priority feature. Will support:
- TOTP (Time-based One-Time Passwords)
- Compatible with Google Authenticator, Authy, etc.
- QR code scanning
- Manual secret entry

### Will there be a Windows/Mac version?

**Maybe.**

- Linux is the primary focus
- Cross-platform considered for v0.3.x+
- Depends on interest and contributors

GTK4 is cross-platform, so technically possible.

### Can you add password sharing?

**Planned for v0.4.x+**

Shared vaults for families/teams are on the roadmap:
- Permission levels (read/edit/admin)
- Secure sharing protocol
- Audit logs

### What about cloud sync?

**Planned for v0.4.x+**

Optional sync support:
- Self-hosted sync server
- End-to-end encryption
- Conflict resolution
- Still usable offline

**Philosophy:** Local-first with optional sync, never cloud-dependent.

---

## Contributing

### How can I help?

Many ways to contribute:

- **Code:** Check issues labeled "good first issue"
- **Testing:** Report bugs, test new features, test with real KeePass/1Password
- **Documentation:** Improve wiki, write guides
- **Translations:** i18n support coming soon
- **Design:** UI/UX improvements, icons, themes
- **Spread the word:** Tell others, write blog posts

See **[[Contributing]]** for details.

### I found a bug, where do I report it?

[GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)

**Include:**
- KeepTower version (v0.2.5-beta, etc.)
- Linux distribution and version
- Steps to reproduce
- Expected vs actual behavior
- Error messages (from terminal if available)

### I have a feature request

Great! Open an issue on GitHub with:
- Clear description of feature
- Use case (why you need it)
- Proposed UI/UX (if applicable)
- Willingness to contribute (optional)

Check roadmap first to see if it's already planned.

---

## Still Have Questions?

- **Wiki:** Browse other pages for detailed info
- **GitHub Issues:** Search existing issues
- **GitHub Discussions:** Ask questions, discuss ideas

---

**Didn't find your answer?** Open a discussion on GitHub and we'll add it to this FAQ!
