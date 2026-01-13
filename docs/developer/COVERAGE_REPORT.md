# KeepTower Code Coverage Report

## Overview

Current code coverage metrics for KeepTower project:

| Metric | Coverage | Target |
|--------|----------|--------|
| **Lines** | **64.4%** (2751/4274) | 70%+ |
| **Functions** | **78.2%** (373/477) | 80%+ |
| **Branches** | **40.1%** (2116/5280) | 60%+ |

**Overall Grade**: **B+** (on track for A+ with targeted improvements)

## Coverage by Component

### Core Business Logic

Well-tested areas:
- ✅ **VaultManager**: Comprehensive tests for vault operations
- ✅ **AccountService/GroupService**: Service layer well covered
- ✅ **Repositories**: Database operations tested
- ✅ **ReedSolomon**: Forward error correction tested
- ✅ **SecureMemory**: Memory security features tested

Areas needing improvement:
- ⚠️ **VaultManagerV2**: New format needs more edge case tests
- ⚠️ **KeyWrapping**: Crypto wrapper error paths undertested
- ⚠️ **PasswordHistory**: Needs comprehensive history tests
- ⚠️ **Crypto/VaultCrypto**: Error handling paths need coverage

### UI Layer

Current status:
- ⚠️ **Controllers**: UI controllers have minimal direct testing
- ⚠️ **Views**: GTK4 widgets tested via integration tests only
- ℹ️ **Note**: UI testing relies on integration tests, which is acceptable

### Utilities

- ✅ **SecureMemory**: Core security primitives well tested
- ✅ **FuzzyMatch**: Search functionality covered
- ✅ **SettingsValidator**: Validation logic tested
- ✅ **Cpp23Compat**: Compatibility layer tested

## Generating Coverage Reports

### Local Development

```bash
# Setup coverage build
meson setup build-coverage -Dcoverage=true -Db_coverage=true --buildtype=debug

# Compile with coverage instrumentation
meson compile -C build-coverage

# Run tests with coverage collection
meson test -C build-coverage

# Generate HTML report
bash scripts/generate-coverage.sh build-coverage

# View report
xdg-open build-coverage/coverage/html/index.html
```

### CI/CD Integration

Coverage is automatically generated on pull requests via GitHub Actions:
- Workflow: `.github/workflows/coverage.yml`
- Platform: Ubuntu 24.04
- Upload: Codecov and Coveralls
- Artifacts: HTML report (30 day retention)

## Improving Coverage

### Priority 1: Core Business Logic (Target: 80% lines)

**VaultManagerV2** (currently 4.6%):
- Add tests for malformed V2 vault handling
- Test upgrade path from V1 to V2
- Error recovery scenarios

**KeyWrapping** (currently 13.7%):
- Test all encryption modes
- Error paths for invalid keys
- Key derivation edge cases

**VaultCrypto** (currently 8.0%):
- PBKDF2 parameter variations
- Encryption/decryption error paths
- Salt generation and validation

### Priority 2: Branch Coverage (Target: 60%)

Current 40.1% indicates many error paths untested:
- Add negative test cases for all core functions
- Test boundary conditions (empty inputs, max sizes)
- Validate error handling for all external calls (OpenSSL, file I/O)

### Priority 3: Utilities (Target: 90%)

- **FuzzyMatch** (7.1%): Comprehensive fuzzy search tests
- **PasswordHistory** (8.7%): History limit, deduplication, serialization

## Test Coverage Best Practices

### Do's ✅
- Test happy path first, then error paths
- Aim for 80%+ line coverage on core business logic
- Focus on branch coverage for critical security code
- Use parameterized tests for similar scenarios
- Mock external dependencies (GTK signals, file I/O)

### Don'ts ❌
- Don't test private implementation details
- Don't chase 100% coverage - diminishing returns after 80-85%
- Don't ignore branch coverage in favor of line coverage
- Don't skip error path testing

## Tools Used

- **gcov**: GCC code coverage tool (built into compiler)
- **lcov**: Front-end for gcov data collection
- **genhtml**: HTML report generator
- **Codecov**: Online coverage tracking
- **Coveralls**: Alternative coverage service

## Notes

### GCC 15 Compatibility

When using GCC 15+ with C++23, lcov may report exception specification mismatches. These are benign and handled with `--ignore-errors mismatch,inconsistent` flags in the coverage script.

### UI Testing Philosophy

KeepTower uses integration tests for UI validation rather than unit testing GTK4 widgets directly. This is by design - GTK widgets are framework code and testing signal connections in isolation provides limited value. The 64% line coverage is still strong given this architectural choice.

### Coverage Targets

- **Core (src/core/)**: Target 80%+ line coverage
- **Utils (src/utils/)**: Target 85%+ line coverage
- **UI (src/ui/)**: Target 50%+ line coverage (integration tests count)
- **Overall Project**: Target 70%+ line coverage for A+ grade

## Next Steps

1. ✅ Integrate coverage into CI/CD (DONE)
2. ⏳ Add VaultManagerV2 comprehensive tests → +5% coverage
3. ⏳ Add KeyWrapping error path tests → +3% coverage
4. ⏳ Add VaultCrypto edge case tests → +3% coverage
5. ⏳ Add FuzzyMatch comprehensive tests → +2% coverage
6. 🎯 **Target: 77%+ line coverage** (from current 64.4%)

This would bring testing quality from **B+** to **A+**.

## Additional Resources

For information on accessing test logs and debugging test runs, see:
- **[FINDING_TEST_LOGS.md](FINDING_TEST_LOGS.md)** - How to locate and access testlog.txt files
