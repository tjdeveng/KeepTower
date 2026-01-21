# Vault Security Algorithms

KeepTower uses state-of-the-art cryptographic algorithms to protect your vault data. This guide explains the key derivation algorithms available when creating new vaults and how to choose the right one for your needs.

## Table of Contents

1. [Understanding Key Derivation](#understanding-key-derivation)
2. [Available Algorithms](#available-algorithms)
3. [Algorithm Comparison](#algorithm-comparison)
4. [FIPS-140-3 Compliance](#fips-140-3-compliance)
5. [Security Recommendations](#security-recommendations)
6. [Advanced Parameters](#advanced-parameters)

---

## Understanding Key Derivation

KeepTower uses **two separate key derivation processes** to secure your vault:

### 1. Username Hashing Algorithm
Determines how usernames are hashed for storage in the vault. This affects:
- Username privacy (hashed usernames are not reversible)
- Username lookup performance
- Vault-wide consistency (all users in a vault share the same algorithm)

### 2. Password Key Encryption Key (KEK) Algorithm
Determines how your master password is converted into an encryption key. This affects:
- Password-to-key strength (resistance to brute-force attacks)
- Login performance (time to derive the key)
- FIPS compliance (some algorithms are FIPS-140-3 validated)

**Important:** For SHA3 variants (SHA3-256, SHA3-384, SHA3-512), KeepTower automatically upgrades the password KEK to **PBKDF2-HMAC-SHA256** because SHA3 alone is too fast for secure password-based encryption. This ensures strong protection regardless of your username hashing choice.

---

## Available Algorithms

### SHA3-256 (FIPS-140-3 Validated) ✓
- **Username**: SHA3-256 (NIST FIPS 202)
- **Password KEK**: PBKDF2-HMAC-SHA256 (NIST SP 800-132)
- **FIPS Compliant**: ✓ Yes
- **Performance**: Fast (< 1ms username hash, ~300ms KEK derivation)
- **Security**: High (600,000 PBKDF2 iterations default)
- **Best For**: Government, healthcare, regulated industries requiring FIPS compliance

### SHA3-384 (FIPS-140-3 Validated) ✓
- **Username**: SHA3-384 (NIST FIPS 202)
- **Password KEK**: PBKDF2-HMAC-SHA256 (NIST SP 800-132)
- **FIPS Compliant**: ✓ Yes
- **Performance**: Fast (< 1ms username hash, ~300ms KEK derivation)
- **Security**: High (600,000 PBKDF2 iterations default)
- **Best For**: Enhanced collision resistance, paranoid security settings

### SHA3-512 (FIPS-140-3 Validated) ✓
- **Username**: SHA3-512 (NIST FIPS 202)
- **Password KEK**: PBKDF2-HMAC-SHA256 (NIST SP 800-132)
- **FIPS Compliant**: ✓ Yes
- **Performance**: Fast (< 1ms username hash, ~300ms KEK derivation)
- **Security**: High (600,000 PBKDF2 iterations default)
- **Best For**: Maximum hash output size, quantum-resistant designs

### PBKDF2-HMAC-SHA256 (FIPS-140-3 Validated) ✓
- **Username**: PBKDF2-HMAC-SHA256 (NIST SP 800-132)
- **Password KEK**: PBKDF2-HMAC-SHA256 (NIST SP 800-132)
- **FIPS Compliant**: ✓ Yes
- **Performance**: Moderate (~300ms for both username and KEK derivation)
- **Security**: High (600,000 iterations default for KEK, 100,000 for username)
- **Best For**: Consistent algorithm for both username and password, maximum FIPS compliance

### Argon2id (Non-FIPS) ⚠️
- **Username**: Argon2id (RFC 9106, Password Hashing Competition winner 2015)
- **Password KEK**: Argon2id (same)
- **FIPS Compliant**: ⚠️ **NO** - Creates a non-FIPS-compliant vault
- **Performance**: Slow (~500ms for both username and KEK derivation with default parameters)
- **Security**: Very High (memory-hard, resistant to GPU/ASIC attacks)
- **Best For**: Personal use, cutting-edge security, environments without FIPS requirements

**Warning:** If you enable FIPS mode in KeepTower settings, Argon2id will be disabled and you cannot create vaults with this algorithm. Existing Argon2id vaults will remain accessible but cannot be migrated to FIPS mode without recreating the vault.

---

## Algorithm Comparison

| Feature | SHA3-256/384/512 + PBKDF2 | PBKDF2 Only | Argon2id |
|---------|---------------------------|-------------|----------|
| **FIPS-140-3 Validated** | ✓ Yes | ✓ Yes | ⚠️ No |
| **Username Hash Speed** | Very Fast (< 1ms) | Moderate (~100ms) | Slow (~500ms) |
| **KEK Derivation Speed** | Moderate (~300ms) | Moderate (~300ms) | Slow (~500ms) |
| **Memory Usage** | Low (~1 MB) | Low (~1 MB) | High (256+ MB) |
| **GPU/ASIC Resistance** | Moderate | Moderate | Very High |
| **Standards Compliance** | NIST FIPS 202, SP 800-132 | NIST SP 800-132 | RFC 9106 |
| **Recommended For** | Regulated industries | Broad compatibility | Personal/advanced users |

### Why SHA3 Requires PBKDF2 for Passwords

SHA3 is a cryptographic hash function designed for **data integrity** and **collision resistance**, not password-based encryption. Key characteristics:

- **Too Fast**: SHA3-256 can hash billions of passwords per second on modern hardware, making brute-force attacks trivial
- **No Memory Hardness**: SHA3 uses minimal memory, allowing attackers to use specialized hardware (ASICs, GPUs) efficiently
- **No Time Stretching**: SHA3 completes in microseconds, providing no computational cost to attackers

**KeepTower's Solution:** When you select a SHA3 variant for username hashing, KeepTower automatically uses **PBKDF2-HMAC-SHA256** (600,000 iterations) for password KEK derivation. This provides:

- **Time Stretching**: 600,000 iterations take ~300ms, slowing brute-force attacks by 6+ orders of magnitude
- **FIPS Compliance**: Both SHA3 and PBKDF2 are FIPS-140-3 validated
- **Best of Both Worlds**: Fast username lookups + strong password protection

---

## FIPS-140-3 Compliance

### What is FIPS-140-3?

FIPS-140-3 (Federal Information Processing Standard Publication 140-3) is a U.S. government security standard that specifies requirements for cryptographic modules. It ensures that cryptographic implementations meet rigorous security and quality standards validated by NIST (National Institute of Standards and Technology).

### Why FIPS Compliance Matters

- **Regulatory Requirements**: Required for U.S. federal agencies and contractors handling sensitive data
- **Industry Standards**: Healthcare (HIPAA), finance, and defense industries often mandate FIPS compliance
- **Security Assurance**: Algorithms undergo extensive third-party validation and testing
- **Risk Reduction**: Reduces liability by using government-approved cryptography

### FIPS Mode in KeepTower

KeepTower supports **optional FIPS mode** (requires OpenSSL 3.5+ with FIPS module):

**When FIPS Mode is Enabled:**
- ✓ SHA3-256, SHA3-384, SHA3-512 algorithms available
- ✓ PBKDF2-HMAC-SHA256 algorithm available
- ⚠️ Argon2id algorithm disabled (automatically reverts to SHA3-256 if selected)
- ✓ All cryptographic operations use FIPS-validated OpenSSL providers
- ✓ Application must be restarted to activate FIPS mode

**When FIPS Mode is Disabled:**
- ✓ All algorithms available (SHA3, PBKDF2, Argon2id)
- ✓ No restrictions on algorithm selection
- ✓ Existing FIPS-compliant vaults remain compliant
- ⚠️ New vaults may use non-FIPS algorithms

### Creating FIPS-Compliant Vaults

To ensure your vault is FIPS-140-3 compliant:

1. **Enable FIPS Mode** (optional but recommended):
   - Open **Preferences → Security**
   - Check "Enable FIPS-140-3 Mode" (requires OpenSSL FIPS module)
   - Restart KeepTower

2. **Select a FIPS Algorithm**:
   - Open **Preferences → Security → Key Derivation Algorithm**
   - Choose one of: SHA3-256, SHA3-384, SHA3-512, or PBKDF2-HMAC-SHA256
   - Avoid Argon2id (non-FIPS)

3. **Create the Vault**:
   - File → New Vault
   - Your vault will be FIPS-compliant

**Checking Vault Compliance:**
- Open **Preferences → Security → Current Vault Security**
- Look for "⚠️ non-FIPS vault" indicator
- FIPS-compliant vaults show "(FIPS)" next to algorithm names

---

## Security Recommendations

### By Use Case

#### Government / Regulated Industries
- **Recommended**: SHA3-256 or PBKDF2-HMAC-SHA256
- **Rationale**: FIPS-140-3 compliance required, fast performance, proven standards
- **Settings**: Enable FIPS mode, use default parameters (600,000 iterations)

#### Healthcare (HIPAA)
- **Recommended**: SHA3-256 or PBKDF2-HMAC-SHA256
- **Rationale**: HIPAA Security Rule recommends NIST-validated cryptography
- **Settings**: Enable FIPS mode, consider increasing PBKDF2 iterations to 1,000,000

#### Enterprise / Business
- **Recommended**: SHA3-256 or PBKDF2-HMAC-SHA256
- **Rationale**: Broad compliance, good performance, auditable standards
- **Settings**: Default parameters, FIPS mode optional based on industry

#### Personal Use (Security-Conscious)
- **Recommended**: Argon2id
- **Rationale**: Cutting-edge security, memory-hard resistance to specialized attacks
- **Settings**: Default parameters (256 MB memory, 4 iterations, 4 threads)

#### Personal Use (Balanced)
- **Recommended**: SHA3-256
- **Rationale**: Fast performance, strong security, future-proof
- **Settings**: Default parameters (600,000 PBKDF2 iterations)

### General Guidelines

1. **Default Settings Are Secure**: KeepTower's defaults (600,000 PBKDF2 iterations, 256 MB Argon2id memory) meet or exceed industry recommendations as of 2024-2025.

2. **Avoid Changing Algorithms After Vault Creation**: You cannot change a vault's KEK algorithm after creation without recreating the vault. Choose carefully.

3. **Balance Security vs. Performance**: Higher iterations/memory = stronger security but slower login times. Test your chosen settings before committing to a vault.

4. **Consider Your Threat Model**:
   - **Nation-state adversaries**: Use Argon2id with maximum parameters (512 MB+ memory)
   - **Standard threats**: SHA3-256 or PBKDF2 with default parameters is sufficient
   - **FIPS compliance**: Use FIPS algorithms regardless of threat level

5. **Use Strong Master Passwords**: No algorithm can compensate for weak passwords. Use 20+ character passphrases or diceware-generated passwords.

---

## Advanced Parameters

### PBKDF2-HMAC-SHA256 Parameters

**Iterations** (default: 600,000 for KEK, 100,000 for username)
- Higher = slower but more secure
- OWASP recommends 600,000+ iterations for SHA-256 (2024)
- Each doubling of iterations doubles login time and attacker cost
- Recommended range: 600,000 - 2,000,000

**How to Adjust:**
1. Preferences → Security → Key Derivation Algorithm
2. Select PBKDF2-HMAC-SHA256
3. Click "Advanced Parameters"
4. Set "PBKDF2 Iterations" (affects both username and KEK)

### Argon2id Parameters

**Memory Cost** (default: 262,144 KB = 256 MB)
- Higher = more memory required, better GPU/ASIC resistance
- Each doubling of memory doubles attacker cost
- Recommended range: 256 MB - 1 GB (system memory permitting)

**Time Cost / Iterations** (default: 4)
- Higher = more computation passes, slower derivation
- Recommended range: 3 - 10

**Parallelism / Threads** (default: 4)
- Number of parallel threads used during derivation
- Should match CPU core count (typically 2-8)
- Does **not** significantly affect security, only performance

**How to Adjust:**
1. Preferences → Security → Key Derivation Algorithm
2. Select Argon2id
3. Click "Advanced Parameters"
4. Set Memory Cost, Time Cost, and Parallelism
5. **Test the performance** before creating the vault

**Performance Warning:** KeepTower will estimate Argon2id login time and warn you if it exceeds 1 second with your chosen parameters. High memory values (512 MB+) may cause noticeable delays on slower systems.

---

## Frequently Asked Questions

### Can I change the algorithm of an existing vault?

**No.** The key derivation algorithm is embedded in the vault format at creation time and cannot be changed. To migrate to a different algorithm, you must:

1. Export your passwords (File → Export)
2. Create a new vault with the desired algorithm
3. Import your passwords into the new vault
4. Securely delete the old vault

### What happens if I open an Argon2id vault with FIPS mode enabled?

The vault will open normally. FIPS mode only restricts **creating new vaults** with non-FIPS algorithms. Existing vaults are not affected.

### Should I use the same algorithm for username and KEK?

Not necessarily. KeepTower automatically chooses the best KEK algorithm:
- **SHA3 variants**: Automatically use PBKDF2 for KEK (stronger password security)
- **PBKDF2**: Uses PBKDF2 for both (consistent)
- **Argon2id**: Uses Argon2id for both (maximum security)

### How do I know if my vault is FIPS-compliant?

Open **Preferences → Security → Current Vault Security**. If you see "⚠️ non-FIPS vault" next to the algorithm name, your vault is not FIPS-compliant. FIPS-compliant vaults show "(FIPS)" after the algorithm name.

### What if my system doesn't have the FIPS module?

You can still create FIPS-compliant vaults without enabling FIPS mode. Simply choose a FIPS algorithm (SHA3 or PBKDF2) when creating the vault. FIPS mode only enforces the use of FIPS-validated OpenSSL providers; the algorithms themselves are FIPS-compliant regardless of mode.

### Is Argon2id less secure because it's not FIPS-validated?

**No.** Argon2id is considered more secure than PBKDF2 for password hashing due to its memory-hard properties. However, it lacks FIPS validation because:
1. It's a newer algorithm (2015) not yet in FIPS standards
2. FIPS validation requires extensive NIST review and approval (years-long process)
3. It's not mandated by government/industry regulations

For personal use or non-regulated environments, Argon2id is the strongest choice. For regulated industries, use FIPS algorithms regardless of technical superiority.

---

## Further Reading

- [NIST FIPS 202: SHA-3 Standard](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf)
- [NIST SP 800-132: PBKDF2 Recommendation](https://csrc.nist.gov/publications/detail/sp/800-132/final)
- [RFC 9106: Argon2 Memory-Hard Function](https://datatracker.ietf.org/doc/html/rfc9106)
- [OWASP Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
- [KeepTower Security Architecture](05-security.md)

---

**Last Updated:** January 2025
**KeepTower Version:** 0.3.3+
