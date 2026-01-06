# Contributing to KeepTower

Thank you for your interest in contributing to KeepTower! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Testing Requirements](#testing-requirements)
- [Commit Guidelines](#commit-guidelines)
- [Pull Request Process](#pull-request-process)
- [Security Issues](#security-issues)

## Code of Conduct

This project adheres to a Code of Conduct that all contributors are expected to follow. Please read [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) before contributing.

## Getting Started

### Prerequisites

- C++23 compatible compiler (GCC 13+ or Clang 16+)
- GTKmm 4.0 (>= 4.10)
- OpenSSL (>= 1.1.0)
- Protocol Buffers (>= 3.0)
- Meson build system
- GTest (for tests)
- Git

### Development Setup

1. **Fork and Clone**
   ```bash
   git clone https://github.com/tjdeveng/KeepTower.git
   cd KeepTower
   ```

2. **Build**
   ```bash
   meson setup build
   meson compile -C build
   ```

3. **Run Tests**
   ```bash
   meson test -C build
   ```

4. **Run the Application**
   ```bash
   ./build/src/keeptower
   ```

## How to Contribute

### Reporting Bugs

Before creating a bug report:
- Check existing issues to avoid duplicates
- Test with the latest version
- Collect relevant information (OS, version, steps to reproduce)

When creating a bug report, include:
- Clear, descriptive title
- Exact steps to reproduce
- Expected vs actual behavior
- Screenshots if applicable
- System information
- Relevant logs or error messages

### Suggesting Enhancements

Enhancement suggestions are welcome! Include:
- Clear description of the proposed feature
- Use cases and benefits
- Potential implementation approach
- Any security implications

### Your First Code Contribution

Good first issues are labeled with `good first issue`. These are:
- Well-defined in scope
- Have clear acceptance criteria
- Don't require deep system knowledge

## Coding Standards

### C++ Style

- **Standard**: C++23 features encouraged
- **Naming**:
  - Classes: `PascalCase`
  - Functions/Methods: `snake_case`
  - Variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Member variables: `m_` prefix (e.g., `m_vault_path`)

- **Formatting**:
  - Indentation: 4 spaces (no tabs)
  - Line length: 100 characters max
  - Braces: Opening brace on same line
  - Pointer/Reference: `Type* ptr`, `Type& ref`

### Modern C++ Best Practices

- Use RAII for resource management
- Prefer `std::unique_ptr` and `std::shared_ptr` over raw pointers
- Use `std::expected` for error handling
- Use `std::span` instead of pointer + size
- Avoid `new`/`delete` in favor of smart pointers
- Use `auto` where type is obvious
- Use range-based for loops
- Use `constexpr` where possible

### Object-Oriented Design Principles

KeepTower follows SOLID principles to maintain clean, maintainable code:

**Single Responsibility Principle (SRP):**
- Each class should have one reason to change
- Avoid "god objects" that do everything
- If a class name contains "And" or "Manager", consider splitting it
- Example: `VaultManager` (vault ops) vs `VaultCrypto` (encryption)

**Open/Closed Principle (OCP):**
- Open for extension, closed for modification
- Use interfaces/abstract classes for extensibility
- Example: `VaultFormat` interface allows V1/V2 implementations

**Liskov Substitution Principle (LSP):**
- Derived classes must be substitutable for base classes
- Honor parent class contracts and invariants

**Interface Segregation Principle (ISP):**
- Many small, focused interfaces over large monolithic ones
- Clients shouldn't depend on unused methods

**Dependency Inversion Principle (DIP):**
- Depend on abstractions, not concretions
- Use dependency injection where appropriate

**Additional Best Practices:**
- **Composition over Inheritance**: Prefer object composition
- **Encapsulation**: Keep implementation private, minimal public interface
- **Const Correctness**: Mark non-mutating methods `const`
- **No Naked News**: Always use smart pointers for ownership
- **Rule of Zero**: Let compiler generate special members when possible

**Example:**
```cpp
// Good: Single responsibility, clear interface
class PasswordValidator {
    [[nodiscard]] bool validate(const std::string& password) const;
    [[nodiscard]] std::vector<std::string> get_issues(const std::string& password) const;
};

// Bad: God object with multiple responsibilities
class VaultManagerAndUIAndCryptoAndEverything {
    void create_vault();
    void show_window();
    void encrypt_data();
    void send_network_request();
    // Violates SRP!
};
```

### Security Considerations

- **Memory Safety**:
  - Clear sensitive data with `OPENSSL_cleanse()`
  - Use `mlock()` for sensitive buffers
  - Avoid memory leaks

- **Input Validation**:
  - Validate all user inputs
  - Check bounds and limits
  - Sanitize file paths

- **Error Handling**:
  - Don't expose sensitive info in errors
  - Log security events appropriately
  - Handle all error conditions

### FIPS-140-3 Compliance Requirements

KeepTower supports optional FIPS-140-3 validated cryptographic operations. When contributing cryptographic code, ensure compliance with these requirements:

**Approved Algorithms Only:**
- **Encryption**: AES-256-GCM (approved), avoid RC4, DES, 3DES
- **Key Derivation**: PBKDF2-HMAC-SHA256/SHA512 (approved), avoid MD5, SHA1
- **Random Number Generation**: Use OpenSSL FIPS-approved DRBG
- **Key Wrapping**: AES Key Wrap (RFC 3394) with 256-bit keys
- **Hashing**: SHA-256, SHA-512 (approved), avoid MD5, SHA1

**OpenSSL FIPS Module:**
- Always use OpenSSL 3.5.0+ APIs
- Check FIPS mode: `FIPS_mode()` returns 1 when enabled
- Use approved functions: `EVP_*` APIs (not deprecated low-level APIs)
- No direct calls to `AES_encrypt()`, use `EVP_EncryptInit_ex()`

**Key Management:**
- Minimum key sizes: AES-256 (256 bits), HMAC (256 bits)
- Secure key generation using FIPS-approved RNG
- Key zeroization: Use `OPENSSL_cleanse()` to clear keys from memory
- No hardcoded keys or weak key derivation

**Self-Tests:**
- Cryptographic operations must pass FIPS Known Answer Tests (KATs)
- Add unit tests that verify FIPS mode compatibility
- Test both FIPS-enabled and FIPS-disabled modes

**Code Examples:**

```cpp
// Good: FIPS-approved AES-256-GCM encryption
EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv);
EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len);

// Bad: Deprecated low-level API (not FIPS-approved)
AES_KEY aes_key;
AES_set_encrypt_key(key, 256, &aes_key);
AES_encrypt(plaintext, ciphertext, &aes_key);

// Good: FIPS-approved PBKDF2
PKCS5_PBKDF2_HMAC(password, password_len, salt, salt_len,
                  100000, EVP_sha256(), key_len, key);

// Bad: Weak algorithm (MD5 not FIPS-approved)
PKCS5_PBKDF2_HMAC(password, password_len, salt, salt_len,
                  1000, EVP_md5(), key_len, key);

// Good: Secure key cleanup (FIPS-approved)
OPENSSL_cleanse(key, key_len);

// Bad: May be optimized away by compiler
memset(key, 0, key_len);

// Good: FIDO2 YubiKey with HMAC-SHA256 (FIPS-approved)
YubiKeyManager ykm;
auto response = ykm.challenge_response(challenge, YubiKeyAlgorithm::HMAC_SHA256, pin);
if (response.success && response.algorithm == YubiKeyAlgorithm::HMAC_SHA256) {
    // Use 32-byte HMAC-SHA256 response
}

// Bad: Legacy HMAC-SHA1 (deprecated, NOT FIPS-approved)
// DO NOT USE: YubiKeyAlgorithm::HMAC_SHA1
```

**Testing FIPS Compliance:**

```bash
# Build with FIPS support
meson setup build -Dfips_mode=true

# Run FIPS tests
meson test -C build test_security_features

# Verify FIPS mode at runtime
./build/src/keeptower --fips-check
```

**Important Notes:**
- FIPS mode is **optional** and not required for normal operation
- Test both FIPS-enabled and disabled modes
- Document any FIPS-related code with clear comments
- See [INSTALL.md](INSTALL.md#fips-140-3-support) for FIPS setup details

**Resources:**
- [NIST FIPS 140-3 Standard](https://csrc.nist.gov/publications/detail/fips/140/3/final)
- [OpenSSL FIPS 3.0 Module](https://www.openssl.org/docs/fips.html)
- [NIST Approved Algorithms](https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program)

### File Organization

#### Source Code Structure

- **Headers**: One class per header file
- **Implementation**: Match header file name
- **Includes**: Order matters
  1. Corresponding header (if .cc file)
  2. C system headers
  3. C++ standard library
  4. External dependencies
  5. Project headers

#### Directory Structure

All files must be placed in their appropriate directories to maintain repository organization:

**Root Directory (/)** - Only essential project files:
- `README.md` - Project overview and quick start
- `CHANGELOG.md` - Version history and release notes
- `CONTRIBUTING.md` - Contribution guidelines (this file)
- `CODE_OF_CONDUCT.md` - Community standards
- `SECURITY.md` - Security policy and reporting
- `INSTALL.md` - Installation instructions
- `ROADMAP.md` - Project roadmap and future plans
- `LICENSE` - GPL-3.0-or-later license
- `meson.build`, `meson_options.txt` - Build configuration
- `.gitignore`, `.editorconfig` - Project configuration

**❌ DO NOT place in root:**
- Implementation details, reviews, summaries, status reports
- Phase documentation, refactoring plans, migration guides
- Test reports, coverage reports, audit documents
- CI/CD implementation details

**Documentation Structure:**

```
docs/
├── api/              # API documentation (Doxygen output)
├── developer/        # Developer documentation
│   ├── CONTRIBUTING.md        # Extended contribution guide
│   ├── CI_CD_READINESS.md     # CI/CD setup documentation
│   ├── FIPS_COMPLIANCE.md     # FIPS implementation details
│   ├── *_IMPLEMENTATION.md    # Feature implementation docs
│   ├── *_MIGRATION.md         # Migration guides
│   └── REFACTOR_*.md          # Refactoring documentation
├── testing/          # Test documentation
│   ├── COVERAGE_*.md          # Coverage reports
│   ├── TEST_*.md              # Test summaries and guides
│   ├── MANUAL_TEST_*.md       # Manual testing procedures
│   └── *_AUDIT.md             # Test audits
├── audits/           # Code quality audits
│   └── *.md                   # Audit reports
├── features/         # Feature documentation
│   └── *.md                   # Feature specifications
├── refactoring/      # Refactoring documentation
│   └── *.md                   # Refactoring plans and summaries
├── releases/         # Release documentation
│   └── v*.md                  # Version-specific release notes
└── user/             # User-facing documentation
    └── *.md                   # User guides and tutorials
```

**File Naming Conventions:**

- **Implementation docs**: `FEATURE_IMPLEMENTATION.md`, `PHASE*_IMPLEMENTATION.md`
- **Migration guides**: `FEATURE_MIGRATION.md`, `OPENSSL_*_MIGRATION.md`
- **Test reports**: `TEST_*_SUMMARY.md`, `COVERAGE_*_REPORT.md`
- **Audits**: `*_AUDIT.md`, `*_ANALYSIS.md`
- **Status reports**: `PHASE*_COMPLETE.md`, `*_FIXES_APPLIED.md`

**Examples:**

✅ **Correct:**
```
docs/developer/FIPS_IMPLEMENTATION.md
docs/testing/COVERAGE_IMPROVEMENT_SUMMARY.md
docs/developer/PHASE2_COMPLETE.md
docs/audits/MEMORY_LOCKING_AUDIT.md
```

❌ **Incorrect:**
```
/FIPS_IMPLEMENTATION.md           # Should be in docs/developer/
/COVERAGE_SUMMARY.md              # Should be in docs/testing/
/PHASE2_COMPLETE.md               # Should be in docs/developer/
/MEMORY_LOCKING_AUDIT.md          # Should be in docs/audits/
```

**When Adding New Documentation:**

1. **Determine document type:**
   - Implementation/design? → `docs/developer/`
   - Testing/coverage? → `docs/testing/`
   - Audit/analysis? → `docs/audits/`
   - Feature specification? → `docs/features/`
   - User guide? → `docs/user/`

2. **Use descriptive names:**
   - Prefix with feature/phase: `BACKUP_SYSTEM_IMPLEMENTATION.md`
   - Include document type: `*_GUIDE.md`, `*_SUMMARY.md`, `*_AUDIT.md`

3. **Update relevant documentation:**
   - Link from main docs when appropriate
   - Add to CHANGELOG.md if user-facing
   - Reference in related code comments

**Why This Matters:**
- Maintains clean, navigable repository structure
- Makes documentation discoverable
- Prevents root directory clutter
- Helps contributors find relevant information quickly
- Facilitates automated documentation tools

### Documentation

- **File Headers**: Include SPDX license identifier
  ```cpp
  // SPDX-License-Identifier: GPL-3.0-or-later
  // SPDX-FileCopyrightText: 2025 tjdeveng
  ```

- **Comments**:
  - Use `//` for single-line comments
  - Document non-obvious logic
  - Explain "why" not "what"
  - Keep comments up-to-date

- **API Documentation**: Document public interfaces clearly

## Testing Requirements

All contributions must include appropriate tests:

### Unit Tests

- Required for all new features
- Test both success and failure cases
- Cover edge cases and boundary conditions
- Aim for high coverage of new code

### Test Guidelines

- One test file per source file (e.g., `Foo.cc` → `test_foo.cc`)
- Use descriptive test names: `TestName_Scenario_ExpectedBehavior`
- Tests must pass before PR can be merged
- No commented-out tests
- Mock external dependencies when appropriate

### Running Tests

```bash
# All tests
meson test -C build

# Specific test
meson test -C build password_validation_test

# Verbose output
meson test -C build -v

# With coverage (if configured)
meson test -C build --coverage
```

## Commit Guidelines

### Commit Messages

Follow conventional commits format:

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types:**
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Code style (formatting, no logic change)
- `refactor`: Code restructuring
- `perf`: Performance improvement
- `test`: Adding or updating tests
- `chore`: Maintenance tasks
- `security`: Security-related changes

**Example:**
```
feat(vault): add support for key file authentication

Implement optional key file authentication alongside password.
Key files are combined with password using HKDF.

Closes #123
```

### Commit Best Practices

- Keep commits atomic and focused
- Write clear, descriptive messages
- Reference issues/PRs when relevant
- Sign commits if possible (`git commit -s`)

## Pull Request Process

### Before Submitting

1. ✅ Code follows style guidelines
2. ✅ All tests pass
3. ✅ New tests added for new features
4. ✅ Documentation updated
5. ✅ No compiler warnings
6. ✅ SPDX headers on new files
7. ✅ CHANGELOG.md updated (for features/fixes)

### PR Submission

1. **Create Feature Branch**
   ```bash
   git checkout -b feature/your-feature-name
   ```

2. **Make Changes**
   - Write code
   - Add tests
   - Update docs

3. **Commit**
   ```bash
   git add .
   git commit -m "feat: your feature description"
   ```

4. **Push**
   ```bash
   git push origin feature/your-feature-name
   ```

5. **Open PR**
   - Use PR template
   - Link related issues
   - Provide clear description
   - Add screenshots if UI changes

### PR Review Process

- Maintainers will review within 7 days
- Address review feedback promptly
- Keep discussions professional and constructive
- Be open to suggestions
- CI must pass before merge

### After PR Merged

- Delete your feature branch
- Update your fork
- Close related issues

## Security Issues

**DO NOT** report security vulnerabilities as public issues!

See [SECURITY.md](SECURITY.md) for proper reporting procedures.

## Questions?

- **General Questions**: Open a GitHub Discussion
- **Bug Reports**: Create an Issue
- **Feature Requests**: Create an Issue with `enhancement` label
- **Security**: See SECURITY.md

## License

By contributing, you agree that your contributions will be licensed under the GNU General Public License v3.0 or later (GPL-3.0-or-later).

All new files must include the SPDX license identifier:
```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng
```

## Recognition

Contributors will be recognized in:
- CHANGELOG.md (for significant contributions)
- Git commit history
- Release notes

Thank you for contributing to KeepTower! 🎉
