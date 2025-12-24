# CI/CD Updates for Recent Changes

## Date
24 December 2025

## Overview
Updated GitHub Actions workflows to ensure all tests pass with recent changes including:
- Memory locking security tests (RLIMIT_MEMLOCK requirements)
- GSettings schema dependencies
- YubiKey password change prompt fixes

## Changes Made

### 1. `.github/workflows/ci.yml` - Main CI Workflow

#### Added Test Environment Setup Step
**Location**: Before "Run tests" step

```yaml
- name: Setup test environment
  run: |
    # Increase memory lock limit for memory locking tests
    # CI runners typically don't have CAP_IPC_LOCK, but we can increase the limit
    ulimit -l 10240  # 10MB
    echo "Memory lock limit: $(ulimit -l)"

    # Ensure GSettings schema is compiled
    ls -la build/data/gschemas.compiled || echo "Schema not found"
```

**Purpose**:
- Sets RLIMIT_MEMLOCK to 10MB for memory locking tests
- Verifies GSettings schema compilation

#### Updated Test Execution
**Location**: "Run tests" step

```yaml
- name: Run tests
  run: |
    # Set GSETTINGS_SCHEMA_DIR for all tests (some may need it)
    export GSETTINGS_SCHEMA_DIR="${{ github.workspace }}/build/data"

    # Run tests with increased verbosity
    meson test -C build --verbose --print-errorlogs
```

**Purpose**:
- Ensures all tests can find GSettings schema
- Adds `--print-errorlogs` for better CI debugging

#### Added Test Summary Display
**Location**: After "Run tests" step

```yaml
- name: Display test summary
  if: always()
  run: |
    echo "=== Test Summary ==="
    if [ -f build/meson-logs/testlog.txt ]; then
      tail -50 build/meson-logs/testlog.txt
    else
      echo "No test log found"
    fi
```

**Purpose**:
- Shows test summary even on failure
- Helps diagnose CI-specific issues

### 2. `.github/workflows/release.yml` - Release Workflow

#### Updated Binary Build Step
**Location**: "Build binary for ${{ matrix.artifact }}" step (line ~125)

```yaml
# Set up test environment
export GSETTINGS_SCHEMA_DIR="${{ github.workspace }}/build/data"
ulimit -l 10240 2>/dev/null || echo "Warning: Could not increase memory lock limit"

# Run tests
meson test -C build --print-errorlogs
```

**Purpose**:
- Ensures release builds are also tested
- Same environment setup as CI workflow

## Test Environment Requirements

### Memory Locking Tests
- **Requirement**: RLIMIT_MEMLOCK ≥ 10MB
- **CI Solution**: `ulimit -l 10240` before tests
- **Graceful Degradation**: Tests handle failure with warnings

### GSettings Schema Tests
- **Requirement**: GSETTINGS_SCHEMA_DIR pointing to compiled schema
- **CI Solution**: `export GSETTINGS_SCHEMA_DIR="$PWD/build/data"`
- **Affected Tests**:
  - Undo/Redo Preferences Tests
  - Settings Validator Tests
  - UI Security Tests

### YubiKey Tests
- **Status**: Mock/skip in CI (no physical hardware)
- **Handling**: Tests skip YubiKey operations when hardware not present
- **Note**: Manual testing still required for YubiKey functionality

## Files Modified
1. `.github/workflows/ci.yml` - Main CI pipeline (3 changes)
2. `.github/workflows/release.yml` - Release pipeline (1 change)

## Verification Checklist

Before pushing:
- [x] All tests pass locally: `meson test -C build`
- [x] Memory locking tests execute: Check for `Memory Locking Security Tests` in output
- [x] Schema-dependent tests pass: Undo/Redo, Settings Validator, UI Security
- [x] YubiKey prompt appears for password changes (manual test)

After pushing:
- [ ] CI workflow completes successfully
- [ ] All 22 tests pass in CI
- [ ] No schema-related errors in logs
- [ ] Memory locking graceful degradation warnings (expected without CAP_IPC_LOCK)

## Expected CI Behavior

### Normal Test Execution
```
Ok:                 22/22
Expected Fail:      0
Fail:               0
Unexpected Pass:    0
Skipped:            0
Timeout:            0
```

### Expected Warnings (Non-Critical)
```
WARN : VaultManager: Failed to increase RLIMIT_MEMLOCK: Operation not permitted
WARN : VaultManager: Memory locking may fail. Run with CAP_IPC_LOCK or increase ulimit -l
```

**Note**: These warnings are expected in CI without CAP_IPC_LOCK capability. Tests verify graceful degradation.

## Additional Notes

### No Changes Needed For:
- **Build Workflow** (`.github/workflows/build.yml`) - Doesn't run tests
- **Dependencies** - No new system packages required
- **Cache Keys** - OpenSSL and libcorrect caches unchanged

### Testing in CI vs Local
| Aspect | Local Dev | CI Environment |
|--------|-----------|----------------|
| RLIMIT_MEMLOCK | Can be set with `setrlimit()` | Limited by ulimit |
| GSettings schema | User dconf may interfere | Clean environment |
| YubiKey hardware | Available if plugged in | Not available |
| CAP_IPC_LOCK | May have capability | No capability |

## Troubleshooting

### If Tests Fail in CI:

1. **Check test logs**: Look at "Display test summary" step output
2. **Schema issues**: Verify `GSETTINGS_SCHEMA_DIR` is set correctly
3. **Memory locking**: Check if `ulimit -l` command succeeded
4. **Individual test failure**: Look for specific test name in logs

### Common Issues:

**"No such schema" error**:
```bash
# Solution: Ensure GSETTINGS_SCHEMA_DIR is exported
export GSETTINGS_SCHEMA_DIR="$PWD/build/data"
```

**Memory locking tests fail**:
```bash
# Solution: Increase ulimit before meson test
ulimit -l 10240
```

**YubiKey tests hang**:
```bash
# Not expected in CI (no hardware)
# If occurs, tests should timeout gracefully
```

## Related Documentation
- [MEMORY_LOCKING_TEST_COVERAGE.md](MEMORY_LOCKING_TEST_COVERAGE.md) - Test coverage details
- [YUBIKEY_PASSWORD_CHANGE_FIX.md](YUBIKEY_PASSWORD_CHANGE_FIX.md) - Password change fix
- [MEMORY_LOCKING_TESTS_COMPLETE.md](MEMORY_LOCKING_TESTS_COMPLETE.md) - Test implementation
