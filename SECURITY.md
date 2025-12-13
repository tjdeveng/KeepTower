# Security Policy

## Supported Versions

The following versions of KeepTower are currently supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Security Features

KeepTower implements multiple layers of security to protect your sensitive data:

### Encryption
- **AES-256-GCM**: Authenticated encryption with associated data (AEAD)
- **PBKDF2-SHA256**: Key derivation with 100,000 iterations (configurable)
- **Random Salt**: Unique 32-byte salt per vault
- **Random IV**: Unique 12-byte initialization vector per encryption operation

### Error Correction
- **Reed-Solomon FEC**: Optional forward error correction for vault files
  - CCSDS RS(255,223) standard encoding
  - Configurable redundancy levels (5-50%)
  - Automatic corruption detection and recovery
  - Protection against bit rot, disk errors, and partial file damage
  - Disabled by default for backward compatibility
  - Can be enabled in Preferences for new vaults

### Memory Protection
- **Secure Clearing**: All sensitive data cleared with `OPENSSL_cleanse()`
- **Memory Locking**: Sensitive buffers locked with `mlock()` to prevent swap exposure
- **Clipboard Protection**: Automatic clipboard clearing after 30 seconds

### File Security
- **Atomic Writes**: Vault files written atomically to prevent corruption
- **Automatic Backups**: Previous vault version backed up on each save
- **Restricted Permissions**: Vault files created with 0600 permissions (owner-only access)
- **File Format Versioning**: Magic header and version for future compatibility

### Input Validation
- **NIST SP 800-63B Compliance**: Password requirements follow federal standards
- **Common Password Prevention**: Dictionary of 10,000+ weak passwords blocked
- **Field Length Limits**: All inputs validated to prevent buffer issues
- **Password Strength Feedback**: Real-time strength calculation

## Reporting a Vulnerability

**Please DO NOT report security vulnerabilities through public GitHub issues.**

If you discover a security vulnerability in KeepTower, please report it privately:

### Reporting Process

1. **Contact**: Email security details to the project maintainer
   - **Email**: security@tjdeveng.com
   - **GitHub**: Create a private security advisory at https://github.com/tjdeveng/keeptower/security/advisories

2. **Information to Include**:
   - Type of vulnerability
   - Affected version(s)
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if available)

3. **Response Timeline**:
   - **Initial Response**: Within 48 hours
   - **Status Update**: Within 7 days
   - **Fix Timeline**: Depends on severity
     - Critical: 1-7 days
     - High: 1-2 weeks
     - Medium: 2-4 weeks
     - Low: Next release

4. **Disclosure Policy**:
   - We follow coordinated disclosure
   - Security fixes released before public disclosure
   - Credit given to reporter (unless anonymity requested)
   - Public disclosure 30 days after fix release

## Security Best Practices for Users

When using KeepTower, follow these recommendations:

### Password Security
- Use unique, strong passwords for each vault
- Avoid reusing vault passwords elsewhere
- Consider using passphrases (4+ random words)
- Enable all available security features

### Vault Security
- Store vault files in encrypted home directories
- Use full-disk encryption on your system
- Keep backups in secure locations
- Don't store vaults on network shares

### System Security
- Keep your system updated
- Use a secure operating system
- Disable swap if possible (for maximum security)
- Log out when leaving your workstation

### Operational Security
- Close vaults when not in use
- Don't take screenshots of passwords
- Clear clipboard after copying passwords
- Be aware of screen recording/shoulder surfing

## Known Security Limitations (v0.1.0-beta)

This beta release has the following acknowledged limitations:

1. **Single-User Focus**: No multi-user access controls
2. **Local Storage Only**: No cloud sync (by design, for security)
3. **No 2FA**: Vault access protected by password only
4. **No Key Files**: Password-only authentication
5. **Beta Software**: This is a beta release; use with appropriate caution

## Cryptographic Dependencies

KeepTower relies on these cryptographic libraries:

- **OpenSSL 3.x**: For AES-256-GCM, PBKDF2, random number generation
- **System RNG**: `/dev/urandom` for salt/IV generation

We regularly monitor security advisories for our dependencies and update promptly.

## Security Review

- **Internal Review**: CODE_REVIEW.md (December 2025)
- **Code Quality**: Multiple security-focused code reviews completed
- **Testing**: Comprehensive unit tests with memory safety checks (valgrind)
- **Open Source**: All code publicly available for community review

## Compliance

KeepTower aims to comply with:
- **NIST SP 800-63B**: Digital Identity Guidelines
- **OWASP Top 10**: Web Application Security
- **CWE Top 25**: Most Dangerous Software Weaknesses

## Updates and Notifications

Security updates will be announced through:
- GitHub Security Advisories
- Release notes (CHANGELOG.md)
- Git tags with security designations

## Questions?

For general security questions (not vulnerability reports), please:
- Open a GitHub Discussion
- Check existing documentation
- Review CODE_REVIEW.md

Thank you for helping keep KeepTower secure!
