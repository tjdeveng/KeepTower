# Security

This document explains how KeepTower protects your passwords and the security measures implemented.

## Table of Contents

- [Security Overview](#security-overview)
- [Encryption](#encryption)
- [Key Derivation](#key-derivation)
- [Memory Protection](#memory-protection)
- [Error Correction](#error-correction)
- [Threat Model](#threat-model)
- [Security Best Practices](#security-best-practices)
- [Reporting Security Issues](#reporting-security-issues)

---

## Security Overview

KeepTower employs multiple layers of security to protect your passwords:

1. **Strong Encryption:** AES-256-GCM authenticated encryption
2. **Key Derivation:** PBKDF2 with 100,000+ iterations
3. **Multi-User Access Control:** Role-based permissions (V2 vaults)
4. **Password History:** Prevents password reuse (V2 vaults)
5. **Memory Protection:** Sensitive data locked in RAM
6. **Data Integrity:** Reed-Solomon forward error correction
7. **Automatic Backups:** Protection against data loss
8. **YubiKey Support:** Hardware authentication (V2 vaults)
9. **No Cloud Dependency:** Your data never leaves your machine

**Security Status:** ✅ Production-ready encryption
**Audit Status:** ⏳ Awaiting third-party security audit
**FIPS 140-3:** Optional FIPS mode available with OpenSSL FIPS provider

---

## Encryption

### Algorithm: AES-256-GCM

**What is it?**
- **AES-256:** Advanced Encryption Standard with 256-bit keys
- **GCM:** Galois/Counter Mode - provides both encryption and authentication
- **Industry Standard:** Used by governments, militaries, and security professionals worldwide

**Why GCM?**
- ✅ **Authenticated Encryption:** Detects tampering or corruption
- ✅ **Proven Security:** NIST-approved, widely analyzed
- ✅ **Performance:** Hardware-accelerated on modern CPUs
- ✅ **Parallel:** Fast encryption/decryption

### Implementation

```
V1 Vault File Structure (Single-User):
┌──────────────────────────────────────┐
│ Salt (32 bytes)                      │  ← Random per vault
├──────────────────────────────────────┤
│ IV (12 bytes)                        │  ← Random per save
├──────────────────────────────────────┤
│ Flags (1 byte)                       │  ← FEC enabled?
├──────────────────────────────────────┤
│ RS Redundancy (1 byte, optional)     │  ← If FEC enabled
├──────────────────────────────────────┤
│ Encrypted Data + Auth Tag            │  ← Your passwords
│ (Reed-Solomon encoded if FEC enabled)│
└──────────────────────────────────────┘

V2 Vault File Structure (Multi-User):
┌──────────────────────────────────────┐
│ Magic Number (4 bytes)               │  ← "KT2\0"
├──────────────────────────────────────┤
│ Version (2 bytes)                    │  ← Format version
├──────────────────────────────────────┤
│ Flags (1 byte)                       │  ← FEC, YubiKey flags
├──────────────────────────────────────┤
│ Security Policy (117 bytes)          │  ← Min length, history depth, etc.
├──────────────────────────────────────┤
│ User Slots (variable)                │  ← Per-user encrypted DEKs
│   ├─ Username, salt, wrapped DEK     │
│   ├─ Role, password history          │
│   └─ YubiKey enrollment info         │
├──────────────────────────────────────┤
│ Data Encryption Key (DEK) encrypted  │  ← Encrypts vault data
├──────────────────────────────────────┤
│ IV (12 bytes)                        │  ← Random per save
├──────────────────────────────────────┤
│ Encrypted Account Data + Auth Tag    │  ← Your passwords
│ (Reed-Solomon encoded if FEC enabled)│
└──────────────────────────────────────┘
```

**Security Properties:**
- **Confidentiality:** Password cannot be read without key
- **Integrity:** Tampering is detected via auth tag
- **Authenticity:** Verifies data came from KeepTower

### Libraries Used

- **OpenSSL 3.x:** Industry-standard cryptography library
- **Audited:** Extensively reviewed and battle-tested
- **FIPS:** Can be built with FIPS 140-2 compliance

---

## Key Derivation

### Master Password → Encryption Key

Your master password is never stored. Instead, it's used to derive the encryption key using **PBKDF2** (Password-Based Key Derivation Function 2).

### PBKDF2 Configuration

```
Key Derivation:
  Algorithm:   PBKDF2-HMAC-SHA256
  Iterations:  100,000
  Salt:        32 bytes (random, unique per vault)
  Output:      256-bit encryption key
```

**Why 100,000 iterations?**
- Makes brute-force attacks expensive (time-consuming)
- OWASP recommends 600,000+ for password hashing, but 100,000 is reasonable for key derivation given the salt
- Balance between security and user experience (unlock time)

**Future:** Consideration for Argon2id (memory-hard algorithm) for even better resistance to GPU/ASIC attacks.

### Salt

Each vault has a unique, random 32-byte salt:
- **Prevents rainbow tables:** Pre-computed attack tables are useless
- **Unique per vault:** Same password in different vaults = different keys
- **Stored in vault file:** Not secret, but essential for key derivation

---

## Memory Protection

### Sensitive Data Handling

KeepTower takes special care with sensitive data in memory:

**Protected Data:**
- Master password (during derivation only)
- Encryption key
- Salt
- Decrypted password data

**Protection Methods:**

1. **Memory Locking (`mlock`)**
   - Prevents swapping to disk
   - Keeps sensitive data in RAM only
   - Protected even if system hibernates

2. **Secure Clearing**
   - Overwrites memory with zeros before freeing
   - Prevents data from lingering in memory
   - Applied to passwords, keys, and salts

3. **Minimal Exposure**
   - Passwords decrypted only when needed
   - Keys kept in memory only while vault is open
   - Clipboard auto-cleared after 45 seconds

### Copy Protection

When you copy a password:
1. Password copied to clipboard
2. Timer starts (45 seconds)
3. Clipboard automatically cleared
4. Prevents accidental paste hours later

---

## Error Correction

### Reed-Solomon Forward Error Correction (FEC)

**Problem:** Storage devices can corrupt data over time:
- **Bit rot:** Random bit flips in storage
- **Bad sectors:** Failing storage hardware
- **Silent corruption:** Errors that go undetected

**Solution:** Reed-Solomon adds redundant data that allows automatic correction.

### How It Works

```
Original Data:     [====== 100 KB ======]
10% Redundancy:    [====== 100 KB ======][=== 10 KB ===]
                    ↑ Original Data        ↑ Parity Data

If corruption occurs in original data:
[==X=== 100 KB ==X===][=== 10 KB ===]
Reed-Solomon can automatically repair the X's
```

### Configuration

**Redundancy Levels:**
- **5%:** Minimal protection (small overhead)
- **10%:** Good balance (recommended)
- **25%:** High protection
- **50%:** Maximum protection (can recover from extensive damage)

**Trade-offs:**
- ✅ **Higher redundancy** = Better protection
- ⚠️ **Higher redundancy** = Larger file size
- ⚠️ **Higher redundancy** = Slightly slower save/load

### When to Enable FEC

**Enable FEC if:**
- ✅ Vault stored on aging hard drives
- ✅ Vault stored on USB flash drives
- ✅ Vault stored on network storage
- ✅ Long-term archival
- ✅ Critical data you can't afford to lose

**FEC less critical if:**
- ⚠️ Vault on modern SSD with SMART monitoring
- ⚠️ Regular backups to multiple locations
- ⚠️ Vault frequently updated (recent backups)

**Recommendation:** Enable with 10-25% redundancy for peace of mind.

---

## Threat Model

### What KeepTower Protects Against

✅ **File Access Attacks**
- Someone gains read access to your vault file
- Protection: Strong encryption (AES-256-GCM)

✅ **Offline Brute-Force Attacks**
- Attacker tries millions of passwords offline
- Protection: PBKDF2 (100,000 iterations) makes each attempt expensive

✅ **Data Corruption**
- Bit rot, bad sectors, storage failures
- Protection: Reed-Solomon error correction

✅ **Memory Dumps**
- Attacker gains access to system memory
- Protection: Memory locking prevents swap exposure

✅ **Shoulder Surfing**
- Someone watching while you enter passwords
- Protection: Passwords hidden by default, temporary show

✅ **Clipboard Snooping**
- Malware reading clipboard
- Protection: Auto-clear after 45 seconds (partial)

### What KeepTower Does NOT Protect Against

❌ **Weak Master Password**
- If your master password is "password123", encryption doesn't help
- **Mitigation:** Use password strength indicator, enforce minimums

❌ **Keyloggers**
- Malware recording keystrokes
- **Mitigation:** Keep system malware-free, use 2FA for critical accounts

❌ **Physical Access to Running System**
- Attacker with physical access while vault is open
- **Mitigation:** Close vault when away, screen lock, full disk encryption

❌ **Compromised Operating System**
- Root-level malware can bypass all protections
- **Mitigation:** Keep system updated, use trusted software sources

❌ **Coercion**
- Being forced to reveal master password
- **Mitigation:** Not solvable by software (legal/physical security)

❌ **Quantum Computing (Future)**
- AES-256 is quantum-resistant, but PBKDF2 may need upgrades
- **Mitigation:** Monitor quantum computing advances, plan migration

### Trust Boundaries

**You must trust:**
- ✅ KeepTower source code (open for review)
- ✅ Your operating system
- ✅ OpenSSL cryptography library
- ✅ Your hardware (CPU, RAM, storage)
- ✅ Your physical security

**You do NOT need to trust:**
- ❌ Cloud providers (no cloud used)
- ❌ Network security (vault never transmitted)
- ❌ Third-party services

---

## Security Best Practices

### Master Password

1. **Use a strong, unique master password**
   - At least 16 characters
   - Mix of character types
   - Not used anywhere else

2. **Memorize it or store securely**
   - Write it down and store in a safe
   - Don't store in another password manager
   - Don't email or text it

3. **No recovery available**
   - Forgetting the master password = permanent data loss
   - This is by design (no backdoor)

### Vault Storage

1. **Store on encrypted drive**
   - Use LUKS/dm-crypt for full disk encryption
   - Protects against physical theft

2. **Regular backups**
   - Enable automatic backups in preferences
   - Periodically copy vault to external storage
   - Test backups by opening them

3. **Secure location**
   - Home directory with proper permissions (`chmod 600`)
   - Not in cloud sync folder (unless encrypted separately)
   - Not on network shares without encryption

### System Security

1. **Keep software updated**
   - Update KeepTower when new versions available
   - Keep OS and libraries patched

2. **Use full disk encryption**
   - Protects vault if device is stolen
   - LUKS for Linux

3. **Lock screen when away**
   - Close vault before leaving computer
   - Enable automatic screen lock

4. **Antivirus/Malware protection**
   - Use ClamAV or similar
   - Be cautious with downloaded software

### Account Security

1. **Use generated passwords**
   - Let KeepTower create strong random passwords
   - 16+ characters recommended

2. **Never reuse passwords**
   - Every account should have unique password
   - If one site breached, others remain safe

3. **Enable 2FA where available**
   - Two-factor authentication adds extra layer
   - TOTP support coming in future KeepTower release

---

## Reporting Security Issues

**Found a security vulnerability?**

Please report it responsibly:

1. **Do NOT create a public GitHub issue**
2. **Email:** (Check repository for security contact)
3. **Include:**
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if you have one)

**Response:**
- We aim to respond within 48 hours
- We'll work with you on a fix
- Credit given in security advisories (if desired)

**Hall of Fame:** Future security researchers who report valid issues will be listed in the repository.

---

## Security Roadmap

Planned security enhancements:

- **Argon2id key derivation** - Memory-hard algorithm
- **Hardware key support** - YubiKey, FIDO2
- **Biometric unlock** - Fingerprint via polkit
- **Security audit** - Third-party professional review
- **TOTP/2FA** - Two-factor auth code generation
- **Breach monitoring** - HaveIBeenPwned integration

See **ROADMAP.md** for timeline.

---

## Security Resources

**Learn more about cryptography:**
- [OWASP Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
- [NIST Special Publication 800-63B](https://pages.nist.gov/800-63-3/sp800-63b.html)
- [Cryptographic Right Answers](https://latacora.micro.blog/2018/04/03/cryptographic-right-answers.html)

**Security best practices:**
- [EFF Surveillance Self-Defense](https://ssd.eff.org/)
- [PrivacyGuides.org](https://www.privacyguides.org/)

---

## Conclusion

KeepTower is designed with security as the top priority. While no system is 100% secure, we follow industry best practices and use proven cryptography to protect your passwords.

**Remember:**
- Your master password is the weakest link - make it strong
- Enable backups and FEC for data protection
- Keep your system secure (updates, screen lock, encryption)
- Report any security concerns responsibly

Stay secure! 🔒
