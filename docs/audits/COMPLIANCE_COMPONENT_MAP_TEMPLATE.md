# Compliance Audit Component Map (Template)

**Project:** KeepTower

**Date:** YYYY-MM-DD

**Purpose:** Identify security-critical components and the depth of review required.

---

## Component Map

| Component | Location(s) | Risk | What to Verify | Tests/Commands | Notes |
|---|---|---:|---|---|---|
| Vault encryption (AES-256-GCM) | `src/...` | High | EVP APIs, tag verification, IV sizing, error handling | `meson test -C build ...` | |
| KDF (PBKDF2) | `src/...` | High | Iteration count, salt size, provider usage in FIPS |  | |
| RNG / IV generation | `src/...` | High | Provider RNG, uniqueness, failure handling |  | |
| Vault file I/O | `src/...` | High | Atomic write, fsync/flush strategy, 0600 perms |  | |
| Backup/restore | `src/...` | Medium | Selection correctness, retention, corruption recovery |  | |
| Secrets lifecycle | `src/...` | High | `OPENSSL_cleanse`, `mlock`, no logs |  | |
| Clipboard handling | `src/ui/...` | Medium | auto-clear, no persistence |  | |
| YubiKey integration | `src/...` | High | HMAC-SHA256, PIN/touch flows, error handling |  | |
| Logging | `src/...` | High | no passwords/keys, avoid sensitive dumps |  | |
| Migration flows | `src/...` | Medium | correctness + determinism + recovery |  | |

---

## Notes
- Add/adjust components based on what’s actually in scope for the audit.
