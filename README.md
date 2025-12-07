# KeepTower Password Manager

[![CI](https://github.com/tjdeveng/KeepTower/workflows/CI/badge.svg)](https://github.com/tjdeveng/KeepTower/actions/workflows/ci.yml)
[![Build](https://github.com/tjdeveng/KeepTower/workflows/Build/badge.svg)](https://github.com/tjdeveng/KeepTower/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-GPL--3.0--or--later-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.0--beta-orange.svg)](CHANGELOG.md)

A secure, modern password manager built with C++23 and GTK4.

## Features

- **Strong Encryption**: AES-256-GCM with authenticated encryption
- **Secure Key Derivation**: PBKDF2-SHA256 with 100,000 iterations (configurable)
- **Error Correction**: Reed-Solomon forward error correction (FEC) for vault files
  - Configurable redundancy levels (5-50%)
  - Automatic corruption detection and recovery
  - Based on CCSDS RS(255,223) standard
- **Memory Protection**: Sensitive data secured with mlock() and OPENSSL_cleanse()
- **Atomic Operations**: Atomic file writes with automatic backups
- **Modern C++23**: Uses std::span, std::expected, RAII throughout
- **GTK4 Interface**: Native GNOME desktop integration
- **Input Validation**: Field length limits enforced on all inputs

## Security Features

- Secure memory clearing prevents data remnants
- Memory locking prevents swap file exposure
- Clipboard auto-clear (30 seconds)
- File permissions restricted to owner only
- Backward-compatible vault format with versioning
- Reed-Solomon error correction protects against bit rot and corruption
- Comprehensive unit test suite (103 tests)

## Building

### Dependencies

- C++23 compatible compiler (GCC 13+ or Clang 16+)
- GTKmm 4.0 (>= 4.10)
- OpenSSL (>= 1.1.0)
- Protocol Buffers (>= 3.0)
- libcorrect (for Reed-Solomon error correction)
- Meson build system
- GTest (for tests)

### Compile

```bash
meson setup build
meson compile -C build
```

### Run Tests

```bash
meson test -C build
```

### Generate API Documentation

Doxygen API documentation can be generated with:

```bash
doxygen Doxyfile
```

The HTML documentation will be created in `docs/api/html/`. Open `docs/api/html/index.html` in your browser to view it.

## Installation

```bash
meson install -C build
```

## Usage

```bash
keeptower
```

### Configuring Reed-Solomon Error Correction

1. Click the **Preferences** button in the toolbar
2. Enable **"Enable error correction for new vaults"**
3. Adjust the **Redundancy Level** slider (5-50%)
   - Higher values provide more protection but increase file size
   - 10% is recommended for most users
   - 20-30% for critical data
4. Click **OK** to save

New vaults created after enabling Reed-Solomon will automatically include error correction. Existing vaults are not modified.

## Documentation

- **README.md** - This file
- **docs/api/** - Doxygen API documentation (generated)
- **CODE_REVIEW.md** - Security audit and code review
- **CONTRIBUTING.md** - Contribution guidelines
- **SECURITY.md** - Security policy and vulnerability reporting

## License

Copyright (C) 2025 tjdeveng

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.

## Third-Party Licenses

KeepTower uses the following open source libraries:

- **GTKmm**: LGPL 2.1 or later
- **OpenSSL**: Apache License 2.0
- **Protocol Buffers**: BSD 3-Clause License
- **Google Test**: BSD 3-Clause License
- **libcorrect**: BSD 3-Clause License

## Contributing

Contributions are welcome! Please ensure:

1. Code follows C++23 best practices
2. All tests pass
3. New features include tests
4. Security-related changes are documented

## Security

For security issues, please contact the maintainer directly rather than using the public issue tracker.

## Architecture

- **Core**: VaultManager handles encryption, key derivation, and vault operations
- **UI**: GTK4-based interface with password dialogs
- **Storage**: Protobuf-based vault format with versioning
- **Tests**: Comprehensive unit and security test suite

## Version

v0.1.0-beta - Initial release
