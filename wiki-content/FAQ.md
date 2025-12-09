# FAQ - Frequently Asked Questions

Common questions about KeepTower answered.

## General Questions

### What is KeepTower?

KeepTower is a password manager for Linux that stores your passwords in an encrypted vault on your local machine. It uses military-grade encryption (AES-256-GCM) and includes optional Reed-Solomon error correction to protect against data corruption.

### Why another password manager?

KeepTower fills a specific niche:
- **Linux-native:** Built with GTK4/libadwaita for deep Linux integration
- **Error correction:** Reed-Solomon FEC protects against bit rot
- **Local-first:** No cloud dependency, your data stays on your machine
- **Modern:** Uses latest GTK4 and follows GNOME HIG
- **Open source:** GPL-3.0 licensed, auditable code

### Is KeepTower ready for production use?

**Current Status:** v0.1.1-beta

- ✅ **Encryption:** Production-ready (AES-256-GCM, PBKDF2)
- ✅ **Core features:** Stable and tested
- ⚠️ **UI/Features:** Still in beta, some features incomplete
- ⏳ **Security audit:** Not yet professionally audited

**Recommendation:** Safe for testing and non-critical use. For mission-critical passwords, wait for v1.0 or use alongside an established manager.

### Is it free?

Yes! KeepTower is **free and open source** software licensed under GPL-3.0-or-later. No subscriptions, no ads, no premium tiers.

---

## Security Questions

### How secure is KeepTower?

KeepTower uses industry-standard encryption:
- **AES-256-GCM:** Same encryption used by governments and militaries
- **PBKDF2:** 100,000 iterations for key derivation
- **Memory protection:** Sensitive data locked in RAM
- **Open source:** Code available for security review

See **[[Security]]** for detailed information.

### What if I forget my master password?

**There is no password recovery.** This is by design - a recovery mechanism would be a security backdoor.

**Recommendations:**
- Write down your master password and store in a safe
- Use a strong but memorable passphrase
- Consider using a password manager for your master password (yes, recursion!)

### Can someone crack my vault with a supercomputer?

**Short answer:** Not with a strong master password.

**Long answer:**
- AES-256 is computationally infeasible to crack by brute force
- PBKDF2 with 100,000 iterations makes password guessing expensive
- **Weak password = weak security** regardless of encryption
- Strong 16+ character password = effectively uncrackable with current technology

### Is my vault safe from bit rot?

**With Reed-Solomon enabled: Yes!**

Reed-Solomon error correction can automatically repair corruption:
- 10% redundancy: Can correct minor corruption
- 25% redundancy: Can correct significant corruption
- 50% redundancy: Can recover from extensive damage

**Without FEC:** Corruption may make vault unopenable.

**Recommendation:** Enable FEC with 10-25% redundancy + keep backups.

### Has KeepTower been security audited?

**Not yet.** A third-party security audit is planned for v1.0.

**Current assurance:**
- Uses well-established cryptography (OpenSSL)
- Open source code available for community review
- Based on industry best practices (OWASP, NIST)

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

**Not yet.** Import functionality is planned for v0.2.x.

**Planned formats:**
- KeePass XML
- 1Password CSV
- Generic CSV
- Chrome/Firefox export

**Workaround:** Manually add accounts for now, or wait for import feature.

### Can I export my passwords?

**Not yet.** Export functionality is planned for v0.2.x.

**Workaround:**
- Vault file is your data - you always have it
- No vendor lock-in
- File format documentation available for custom tools

### Does KeepTower work with browser extensions?

**Not yet.** Browser extension is planned for v0.2.x.

**Current workaround:**
- Copy password with Ctrl+C
- Paste into browser (Ctrl+V)
- Auto-clear prevents long-term clipboard exposure

### Can I use KeepTower on my phone?

**Not yet.** Mobile apps are planned for v0.4.x+.

**Planned platforms:**
- Linux phones (Phosh/Plasma Mobile) - First priority
- Android - Under consideration
- iOS - Maybe in the future

---

## Technical Questions

### What file format does KeepTower use?

**Custom encrypted format:**
- Not compatible with KeePass or other managers
- Binary format (not text/XML)
- Structure: Salt + IV + Flags + Encrypted(Protobuf) + Auth Tag
- Optionally Reed-Solomon encoded

**Why custom format?**
- Allows Reed-Solomon error correction
- Authenticated encryption (GCM)
- Smaller file size (binary vs XML)

**Future:** Import/export to standard formats planned.

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
- Modern Linux distribution (2023+)
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

2. **Corrupted vault file**
   - Try opening a backup
   - Check file size (corrupted = unusual size)
   - If FEC enabled, try again (may auto-repair)

3. **Wrong file**
   - Ensure it's actually a `.vault` file
   - Check creation date

### "Cannot load vault" error

Try these steps:

1. Check file permissions: `ls -l vault.vault`
2. Ensure you own the file: `chown $USER vault.vault`
3. Check available disk space: `df -h`
4. Try opening a backup
5. Report issue on GitHub with error details

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

**Planned for v0.2.x!**

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
- **Testing:** Report bugs, test new features
- **Documentation:** Improve wiki, write guides
- **Translations:** i18n support coming soon
- **Design:** UI/UX improvements, icons, themes
- **Spread the word:** Tell others, write blog posts

See **[[Contributing]]** for details.

### I found a bug, where do I report it?

[GitHub Issues](https://github.com/tjdeveng/KeepTower/issues)

**Include:**
- KeepTower version
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
- **Email:** Check repository for contact information

---

**Didn't find your answer?** Open a discussion on GitHub and we'll add it to this FAQ!
