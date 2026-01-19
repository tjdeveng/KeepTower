# Username Hashing: Algorithm Comparison & Performance

Quick reference guide for selecting the right username hashing algorithm in KeepTower.

## Quick Comparison

| Algorithm | FIPS? | Speed | Security | Best For |
|-----------|-------|-------|----------|----------|
| **SHA3-256** ⭐ | ✅ | Very Fast | High | Most users, FIPS compliance |
| **SHA3-384** | ✅ | Fast | Very High | FIPS + extra security |
| **SHA3-512** | ✅ | Fast | Maximum | FIPS + maximum hash length |
| **PBKDF2-SHA256** | ✅ | Slow | High+ | FIPS + GPU resistance |
| **Argon2id** | ❌ | Very Slow | Maximum | No FIPS, maximum security |
| ~~Plaintext~~ | ❌ | N/A | None | Legacy only |

⭐ = Recommended default

## Performance Impact

Authentication time for single-user vault:

- **SHA3**: < 5 ms (instant)
- **PBKDF2** (100K iterations): ~250 ms (barely noticeable)
- **Argon2id** (64 MB): ~500 ms (noticeable but acceptable)

For 10-user vault, multiply by ~10.

## Decision Tree

```
Do you need FIPS compliance?
├─ YES → Use SHA3-256 (default) or PBKDF2 for high security
└─ NO → Do you need maximum security?
    ├─ YES → Use Argon2id
    └─ NO → Use SHA3-256 (fastest)
```

## Detailed Comparison

### SHA3-256 (Recommended Default)
✅ FIPS-approved
✅ Very fast (~1.5 µs)
✅ Simple, no tuning needed
✅ Excellent for most use cases
❌ Less resistant to GPU attacks than PBKDF2/Argon2

**Use when**: You want a fast, secure, maintenance-free option with FIPS compliance.

### PBKDF2-SHA256
✅ FIPS-approved
✅ GPU-resistant
✅ Tunable (10K-1M iterations)
❌ Slower (~250 ms at 100K iterations)
❌ Not memory-hard (less GPU-resistant than Argon2)

**Use when**: You need FIPS compliance + stronger brute-force protection.

**Tuning**:
- 100,000 iterations (default): ~250 ms, recommended
- 250,000 iterations: ~650 ms, high security
- 1,000,000 iterations: ~2.5 sec, maximum FIPS-compliant security

### Argon2id
✅ Maximum security (memory-hard)
✅ Highly GPU/ASIC-resistant
✅ Tunable (memory + time cost)
❌ Not FIPS-approved
❌ Slowest (~500 ms+)
❌ High memory usage

**Use when**: Maximum security is priority and FIPS is not required.

**Tuning**:
- 64 MB, 3 iterations (default): ~500 ms, recommended
- 256 MB, 5 iterations: ~1.8 sec, maximum security

## Real-World Attack Costs

Time for attacker to compute 1 billion username hashes:

| Algorithm | CPU (Single) | GPU Cluster | ASIC |
|-----------|-------------|-------------|------|
| SHA3-256 | 15 minutes | 5 seconds | < 1 sec |
| PBKDF2 (100K) | 3 years | 1 month | 1 week |
| Argon2id (64MB) | 15 years | 5 years | 2 years |

*Argon2id's memory requirements drastically reduce parallelization benefits.*

## Configuration

### Via Preferences Dialog
1. Open KeepTower → Preferences → Vault Security
2. Select "Username Hashing Algorithm"
3. For PBKDF2/Argon2id: Expand "Advanced Parameters" to tune

### Advanced Parameters

**PBKDF2 Iterations** (10,000 - 1,000,000):
- Default: 100,000 (good balance)
- Increase for stronger security (slower authentication)

**Argon2 Memory Cost** (8 MB - 1 GB):
- Default: 64 MB (good balance)
- Increase for GPU resistance (requires more RAM)

**Argon2 Time Cost** (1 - 10 iterations):
- Default: 3 (good balance)
- Increase for additional computational work

## Important Notes

⚠️ **Algorithm is set at vault creation**
Changing preferences only affects NEW vaults. Existing vaults continue using their original algorithm.

⚠️ **FIPS Mode Enforcement**
When FIPS mode is enabled, only SHA3 and PBKDF2 can be selected. Argon2id will be hidden.

⚠️ **Multi-User Performance**
Authentication time scales with number of users. 10-user vault with Argon2id = 5-10 seconds to display user list.

## Recommendations by Use Case

### Personal Use (No FIPS Required)
- **Default**: SHA3-256 (fast, secure)
- **High Security**: Argon2id (64 MB, 3 iterations)

### Small Business / Team
- **Default**: SHA3-256 (fast, FIPS-ready)
- **Sensitive Data**: PBKDF2 (100K-250K iterations)

### Enterprise / Government
- **FIPS Required**: SHA3-256 (default) or PBKDF2 (high security)
- **FIPS Optional**: Argon2id for maximum security

### Security Researcher / High-Value Targets
- **Maximum Security**: Argon2id (256 MB, 5 iterations)
- Accept slower authentication for maximum protection

## Migration

**Q: Can I change algorithm for existing vault?**
**A**: Not yet. Phase 4 will introduce Migration Tool. Currently, algorithm is fixed at vault creation.

**Workaround**: Create new vault with desired algorithm, manually transfer data.

## Further Reading

- [Full Documentation](../docs/USERNAME_HASHING_ALGORITHMS.md) - Detailed technical analysis
- [FIPS Setup Guide](../docs/YUBIKEY_FIPS_SETUP.md) - FIPS 140-3 configuration
- [Security Best Practices](../docs/SECURITY_BEST_PRACTICES.md) - Overall security guidance

---

**See Also**: [Phase 2 Implementation Plan](../USERNAME_HASH_PREFERENCES_UI_PLAN.md)
