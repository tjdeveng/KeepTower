# Contributing to KeepTower

Thank you for considering contributing to KeepTower! This guide will help you get started.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
- [Development Setup](#development-setup)
- [Coding Guidelines](#coding-guidelines)
- [Submitting Changes](#submitting-changes)
- [Testing](#testing)
- [Documentation](#documentation)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inclusive environment for everyone, regardless of:
- Experience level
- Gender identity and expression
- Sexual orientation
- Disability
- Personal appearance
- Body size
- Race or ethnicity
- Age
- Religion or lack thereof
- Nationality

### Expected Behavior

- ✅ Be respectful and constructive
- ✅ Welcome newcomers and help them learn
- ✅ Accept constructive criticism gracefully
- ✅ Focus on what's best for the project
- ✅ Show empathy towards other contributors

### Unacceptable Behavior

- ❌ Harassment, insults, or discriminatory comments
- ❌ Trolling or inflammatory comments
- ❌ Personal or political attacks
- ❌ Publishing others' private information
- ❌ Other conduct inappropriate in a professional setting

**Enforcement:** Violations may result in temporary or permanent ban from the project.

---

## How Can I Contribute?

### Reporting Bugs

**Before submitting:**
1. Check if the bug is already reported
2. Test with the latest version
3. Collect relevant information

**Bug report should include:**
- KeepTower version
- Linux distribution and version
- Clear steps to reproduce
- Expected behavior
- Actual behavior
- Error messages/logs
- Screenshots (if UI-related)

**Submit via:** [GitHub Issues](https://github.com/tjdeveng/KeepTower/issues/new)

### Suggesting Features

**Before suggesting:**
1. Check if already requested
2. Review the roadmap (ROADMAP.md)
3. Consider if it fits KeepTower's philosophy

**Feature request should include:**
- Clear description
- Use case (why it's needed)
- Proposed UI/UX
- Any implementation ideas
- Willingness to contribute code

### Writing Code

**Good first issues:**
- Look for issues labeled `good first issue`
- These are beginner-friendly tasks
- Mentorship available if needed

**Areas needing help:**
- UI improvements
- Test coverage
- Documentation
- Bug fixes
- Performance optimization

### Improving Documentation

Documentation is crucial! You can help by:
- Fixing typos or unclear wording
- Adding examples
- Writing tutorials
- Translating (when i18n is ready)
- Creating video guides

**Wiki edits:** Can be done directly on GitHub

### Testing

Help test new features and releases:
- Install beta/RC versions
- Test on different distributions
- Report any issues found
- Provide feedback on usability

---

## Development Setup

### Prerequisites

Install development dependencies:

**Fedora/RHEL:**
```bash
sudo dnf install gtkmm4.0-devel protobuf-devel openssl-devel \
    libcorrect-devel meson ninja-build gcc-c++ git \
    gtest-devel doxygen
```

**Ubuntu/Debian:**
```bash
sudo apt install libgtkmm-4.0-dev libprotobuf-dev protobuf-compiler \
    libssl-dev libcorrect-dev meson ninja-build g++ git \
    libgtest-dev doxygen
```

**Arch Linux:**
```bash
sudo pacman -S gtkmm-4.0 protobuf openssl libcorrect meson ninja gcc git gtest doxygen
```

### Clone Repository

```bash
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower
```

### Build

```bash
# Debug build (recommended for development)
meson setup build --buildtype=debug
meson compile -C build

# Run
./build/src/keeptower
```

### Run Tests

```bash
meson test -C build
```

All tests should pass before submitting changes.

---

## Coding Guidelines

### Code Style

**General principles:**
- Follow existing code style
- Use meaningful variable/function names
- Keep functions focused and small
- Comment complex logic

### Object-Oriented Design Principles

KeepTower follows SOLID principles and modern OOP best practices:

**Single Responsibility Principle (SRP):**
- Each class should have one, and only one, reason to change
- Avoid "god objects" that try to do everything
- Example: `VaultManager` handles vault operations, `VaultCrypto` handles encryption
- If a class name has "And" or "Manager", consider splitting it

**Open/Closed Principle (OCP):**
- Classes should be open for extension, closed for modification
- Use interfaces and abstract base classes for extensibility
- Example: `VaultFormat` interface allows V1/V2 vault implementations

**Liskov Substitution Principle (LSP):**
- Derived classes must be substitutable for their base classes
- Don't break parent class contracts in derived classes
- Ensure derived classes honor preconditions/postconditions

**Interface Segregation Principle (ISP):**
- Prefer small, focused interfaces over large, monolithic ones
- Clients shouldn't depend on methods they don't use
- Example: Separate read/write interfaces rather than one large interface

**Dependency Inversion Principle (DIP):**
- Depend on abstractions, not concretions
- High-level modules shouldn't depend on low-level modules
- Use dependency injection where appropriate

**Additional OOP Best Practices:**

- **Composition over Inheritance:** Prefer composing objects over deep inheritance hierarchies
- **Encapsulation:** Keep implementation details private, expose minimal public interface
- **Immutability:** Prefer `const` methods and immutable objects where possible
- **RAII (Resource Acquisition Is Initialization):** Use constructors/destructors for resource management
- **No Raw Pointers:** Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) for ownership
- **Const Correctness:** Mark methods `const` when they don't modify state
- **Rule of Zero/Five:** Either define all special members or none (let compiler generate them)

**Class Design Guidelines:**

```cpp
// Good: Single responsibility, clear purpose
class PasswordValidator {
    bool validate(const std::string& password) const;
    std::vector<std::string> get_strength_issues(const std::string& password) const;
};

// Bad: Multiple responsibilities (god object)
class VaultManagerAndUIAndCryptoAndEverything {
    void create_vault();
    void show_window();
    void encrypt_data();
    void send_network_request();
    // ... hundreds of methods
};

// Good: Clear ownership with unique_ptr
class AccountManager {
    std::unique_ptr<Account> m_active_account;
};

// Good: Const correctness
class VaultReader {
    [[nodiscard]] const Account* get_account(size_t index) const;
    [[nodiscard]] size_t get_account_count() const;
};
```

**C++ Style:**
- **Indentation:** 4 spaces (no tabs)
- **Braces:** Opening brace on same line
  ```cpp
  void function() {
      // code
  }
  ```
- **Naming:**
  - Classes: `PascalCase` (e.g., `VaultManager`)
  - Functions/methods: `snake_case` (e.g., `create_vault`)
  - Member variables: `m_` prefix (e.g., `m_vault_open`)
  - Constants: `UPPER_CASE` (e.g., `KEY_LENGTH`)
- **Headers:** Include guards or `#pragma once`
- **Modern C++:** Use C++23 features appropriately

**GTK/gtkmm:**
- Follow GNOME HIG (Human Interface Guidelines)
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- Prefer `Gtk::make_managed` for widgets

### File Organization

```
src/
├── main.cc                 # Entry point
├── application/            # Application class
├── core/                   # Business logic
│   ├── VaultManager.cc     # Vault operations
│   ├── models/             # Data models
│   ├── services/           # Services
│   └── controllers/        # Controllers
├── ui/                     # User interface
│   ├── windows/            # Main windows
│   ├── dialogs/            # Dialogs
│   └── widgets/            # Custom widgets
└── utils/                  # Utilities
    ├── Log.h               # Logging
    └── helpers/            # Helper functions
```

**When adding new files:**
- Place in appropriate directory
- Update `meson.build`
- Add header comments with license

### FIPS-140-3 Compliance

KeepTower supports optional FIPS-140-3 validated cryptography. Contributors must follow these requirements when working with cryptographic code:

**Approved Algorithms:**
- ✅ **Encryption**: AES-256-GCM
- ✅ **Key Derivation**: PBKDF2-HMAC-SHA256/SHA512
- ✅ **Hashing**: SHA-256, SHA-512
- ✅ **Key Wrapping**: AES Key Wrap (RFC 3394)
- ✅ **RNG**: OpenSSL FIPS DRBG
- ❌ **Not Allowed**: MD5, SHA1, RC4, DES, 3DES

**OpenSSL Requirements:**
- Use OpenSSL 3.5.0+ APIs exclusively
- Use high-level `EVP_*` APIs (not deprecated low-level functions)
- Check FIPS mode: `FIPS_mode()` before FIPS-only operations
- Always use `OPENSSL_cleanse()` for key zeroization

**Key Requirements:**
- Minimum key sizes: AES-256 (256 bits), HMAC (256 bits)
- No hardcoded keys or weak derivation
- Secure random generation using FIPS-approved RNG
- Proper key cleanup with `OPENSSL_cleanse()`

**Example:**
```cpp
// ✅ GOOD: FIPS-approved EVP API
EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv);

// ❌ BAD: Deprecated low-level API
AES_KEY aes_key;
AES_set_encrypt_key(key, 256, &aes_key);
```

**Testing:**
- Test both FIPS-enabled and disabled modes
- Run `meson test -C build test_security_features`
- Verify FIPS KATs pass

**Resources:**
- See [INSTALL.md](../../INSTALL.md#fips-140-3-support) for setup
- [NIST FIPS 140-3](https://csrc.nist.gov/publications/detail/fips/140/3/final)
- [OpenSSL FIPS Module](https://www.openssl.org/docs/fips.html)

### Commit Messages

Follow conventional commits format:

```
<type>: <short description>

<detailed description if needed>

<footer with issue references>
```

**Types:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `style:` Code style (no functional change)
- `refactor:` Code restructuring
- `test:` Adding/updating tests
- `chore:` Maintenance tasks

**Examples:**
```
feat: Add TOTP support for 2FA codes

Implements TOTP generation using HMAC-SHA1.
Allows users to store 2FA secrets and generate codes.

Closes #42
```

```
fix: Prevent crash when opening corrupted vault

Added error handling for malformed vault files.
Now shows user-friendly error message instead of crashing.

Fixes #67
```

### Error Handling

- Use exceptions for exceptional cases
- Return `std::expected<T, Error>` for expected failures
- Log errors appropriately
- Provide user-friendly error messages

**Example:**
```cpp
auto VaultManager::open_vault(const std::string& path) -> std::expected<void, VaultError> {
    if (!fs::exists(path)) {
        return std::unexpected(VaultError::FileNotFound);
    }
    // ...
}
```

### Security Considerations

When working with sensitive data:
- ✅ Use secure memory (`mlock`, secure clearing)
- ✅ Minimize time data is in memory
- ✅ Never log passwords or keys
- ✅ Use cryptographically secure random numbers
- ✅ Follow principle of least privilege

**Review checklist:**
- [ ] No hardcoded secrets
- [ ] Proper memory clearing
- [ ] Input validation
- [ ] No plaintext password storage
- [ ] Cryptographic operations use OpenSSL

---

## Submitting Changes

### Pull Request Process

1. **Fork the repository**
   - Click "Fork" on GitHub
   - Clone your fork locally

2. **Create a branch**
   ```bash
   git checkout -b feature/my-feature
   # or
   git checkout -b fix/my-bugfix
   ```

3. **Make your changes**
   - Write code
   - Add tests
   - Update documentation

4. **Test thoroughly**
   ```bash
   meson test -C build
   # Manual testing
   ```

5. **Commit with clear messages**
   ```bash
   git add .
   git commit -m "feat: Add my feature"
   ```

6. **Push to your fork**
   ```bash
   git push origin feature/my-feature
   ```

7. **Open Pull Request**
   - Go to original repository
   - Click "New Pull Request"
   - Select your branch
   - Fill out PR template

### Pull Request Guidelines

**PR should include:**
- Clear description of changes
- Reference to related issues
- Screenshots (if UI changes)
- Test results
- Documentation updates

**PR checklist:**
- [ ] Code follows style guidelines
- [ ] All tests pass
- [ ] New tests added for new features
- [ ] Documentation updated
- [ ] No merge conflicts
- [ ] Commit messages are clear

**Review process:**
- Maintainers will review your PR
- May request changes
- Be responsive to feedback
- Once approved, will be merged

---

## Testing

### Running Tests

```bash
# Run all tests
meson test -C build

# Run specific test
./build/tests/vault_manager_test

# Run with verbose output
meson test -C build --verbose
```

### Writing Tests

Tests use Google Test framework:

```cpp
#include <gtest/gtest.h>
#include "VaultManager.h"

TEST(VaultManagerTest, CreateVault_Success) {
    VaultManager manager;
    ASSERT_TRUE(manager.create_vault("test.vault", "password123"));
    EXPECT_TRUE(manager.is_vault_open());
}
```

**Test file naming:**
- Place in `tests/` directory
- Name: `test_<feature>.cc`
- Update `tests/meson.build`

**Test guidelines:**
- Test one thing per test
- Use descriptive test names
- Clean up test files in TearDown
- Mock external dependencies when appropriate

### Code Coverage

```bash
# Build with coverage
meson setup build --buildtype=debug -Db_coverage=true
meson compile -C build
meson test -C build

# Generate coverage report
ninja -C build coverage
```

**Target:** >80% code coverage for new code

---

## Documentation

### Code Documentation

Use Doxygen comments:

```cpp
/**
 * @brief Creates a new encrypted vault
 *
 * @param path Filesystem path for the vault file
 * @param password Master password for encryption
 * @return true if successful, false otherwise
 */
bool create_vault(const std::string& path, const Glib::ustring& password);
```

**Generate API docs:**
```bash
meson compile -C build doxygen
# Output in build/docs/html/
```

### User Documentation

- **Wiki:** For user-facing documentation
- **README:** Project overview and quick start
- **ROADMAP:** Feature planning
- **This file:** Contributor guide

**When adding features:**
- Update relevant wiki pages
- Add usage examples
- Update screenshots if needed

---

## Getting Help

### Resources

- **Codebase:** Read existing code for patterns
- **Issues:** Search for similar questions/problems
- **Discussions:** Ask questions
- **Maintainers:** Reach out if stuck

### Questions?

Don't hesitate to ask:
- Open a GitHub Discussion
- Comment on relevant issue
- Reach out to maintainers

**No question is too simple!** We were all beginners once.

---

## Recognition

Contributors are recognized:
- Listed in `CONTRIBUTORS.md` (coming soon)
- Mentioned in release notes
- Credit in commit history
- Eternal gratitude from users! 🙏

---

## License

By contributing, you agree that your contributions will be licensed under GPL-3.0-or-later, the same license as KeepTower.

---

**Thank you for contributing to KeepTower!** 🎉

Your efforts help make password management more secure and accessible for Linux users everywhere.
