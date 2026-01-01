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

### File Organization

- **Headers**: One class per header file
- **Implementation**: Match header file name
- **Includes**: Order matters
  1. Corresponding header (if .cc file)
  2. C system headers
  3. C++ standard library
  4. External dependencies
  5. Project headers

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
