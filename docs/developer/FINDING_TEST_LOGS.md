# Finding Test Logs in KeepTower

## Overview

When running tests with Meson, test execution logs are stored in `testlog.txt` files. This guide explains how to find and access these logs.

## Test Log Location

The `testlog.txt` file is located in the Meson logs directory within your build folder:

```
<build-directory>/meson-logs/testlog.txt
```

For example:
- **Standard build**: `build/meson-logs/testlog.txt`
- **Coverage build**: `build-coverage/meson-logs/testlog.txt`
- **Debug build**: `builddir/meson-logs/testlog.txt`

## How to Find testlog.txt

### Method 1: Direct Path Access

If you know your build directory name, navigate directly to the file:

```bash
# For standard builds
cat build/meson-logs/testlog.txt

# For coverage builds
cat build-coverage/meson-logs/testlog.txt
```

### Method 2: After Running Tests

The file is created or updated each time you run tests. To generate it:

```bash
# Step 1: Setup build (if not already done)
meson setup build

# Step 2: Compile the project
meson compile -C build

# Step 3: Run tests (this creates/updates testlog.txt)
meson test -C build

# Step 4: View the test log
cat build/meson-logs/testlog.txt
```

### Method 3: For Coverage Builds

To generate the testlog.txt in the `build-coverage` directory:

```bash
# Step 1: Setup coverage build
meson setup build-coverage -Dcoverage=true -Db_coverage=true --buildtype=debug

# Step 2: Compile with coverage instrumentation
meson compile -C build-coverage

# Step 3: Run tests (creates testlog.txt)
meson test -C build-coverage

# Step 4: View the test log
cat build-coverage/meson-logs/testlog.txt
```

### Method 4: Using find Command

If you're not sure where your build directory is:

```bash
# Find all testlog.txt files in the project
find . -name "testlog.txt" -path "*/meson-logs/*" 2>/dev/null

# This will output paths like:
# ./build/meson-logs/testlog.txt
# ./build-coverage/meson-logs/testlog.txt
```

### Method 5: List Recent Test Logs

To find the most recently updated test log:

```bash
# Find and sort by modification time
find . -name "testlog.txt" -path "*/meson-logs/*" -exec ls -lt {} + 2>/dev/null
```

## Understanding the Test Log

The `testlog.txt` file contains:

- **Test names**: Each test that was executed
- **Test status**: OK, FAIL, SKIP, TIMEOUT
- **Execution time**: How long each test took
- **Test output**: Any output from failed tests
- **Summary**: Total tests run, passed, failed, skipped

### Example Test Log Content

```
1/103 keeptower:unit / SecureMemoryTest                    OK       0.15s
2/103 keeptower:unit / VaultManagerTest                    OK       0.23s
3/103 keeptower:unit / AccountServiceTest                  OK       0.18s
...
103/103 keeptower:integration / IntegrationTest            OK       1.45s

Ok:                 103
Expected Fail:        0
Fail:                 0
Unexpected Pass:      0
Skipped:              0
Timeout:              0

Full log written to /path/to/build/meson-logs/testlog.txt
```

## Viewing Test Results

### View Full Log

```bash
# View entire log
less build/meson-logs/testlog.txt

# Or with cat
cat build/meson-logs/testlog.txt
```

### View Only Failed Tests

```bash
# Filter for failed tests
grep -A 5 "FAIL" build/meson-logs/testlog.txt
```

### View Test Summary

```bash
# Show just the summary at the end
tail -n 20 build/meson-logs/testlog.txt
```

### View Specific Test

```bash
# Search for a specific test
grep -A 10 "VaultManagerTest" build/meson-logs/testlog.txt
```

## Common Issues and Solutions

### Issue: testlog.txt Does Not Exist

**Cause**: Tests have not been run yet, or build directory doesn't exist.

**Solution**:
```bash
# Create build directory and run tests
meson setup build
meson compile -C build
meson test -C build
# Now testlog.txt should exist at build/meson-logs/testlog.txt
```

### Issue: Can't Find build-coverage Directory

**Cause**: Coverage build has not been set up.

**Solution**:
```bash
# Create coverage build directory
meson setup build-coverage -Dcoverage=true -Db_coverage=true --buildtype=debug
meson compile -C build-coverage
meson test -C build-coverage
# Now testlog.txt should exist at build-coverage/meson-logs/testlog.txt
```

### Issue: testlog.txt is Empty or Outdated

**Cause**: Tests need to be re-run to update the log.

**Solution**:
```bash
# Re-run tests to update the log
meson test -C build-coverage
```

## CI/CD Integration

In GitHub Actions (`.github/workflows/coverage.yml`), the test log is automatically generated:

```yaml
- name: Run tests
  run: |
    xvfb-run -a meson test -C build-coverage --print-errorlogs
```

After this step runs, the file exists at:
```
$GITHUB_WORKSPACE/build-coverage/meson-logs/testlog.txt
```

You can access it in subsequent workflow steps or download it as an artifact.

## Additional Resources

- **Meson Test Documentation**: https://mesonbuild.com/Unit-tests.html
- **KeepTower Testing Guide**: [TESTING_UPGRADE.md](TESTING_UPGRADE.md)
- **Coverage Report Guide**: [COVERAGE_REPORT.md](COVERAGE_REPORT.md)

## Quick Reference

| Build Type | Test Log Path |
|------------|---------------|
| Standard | `build/meson-logs/testlog.txt` |
| Coverage | `build-coverage/meson-logs/testlog.txt` |
| Debug | `builddir/meson-logs/testlog.txt` |
| Custom | `<build-dir>/meson-logs/testlog.txt` |

## Summary

To find `/home/runner/work/KeepTower/KeepTower/build-coverage/meson-logs/testlog.txt`:

1. The file is created when you run `meson test -C build-coverage`
2. It requires the coverage build to be set up first
3. The file path is always: `<build-directory>/meson-logs/testlog.txt`
4. Use `find . -name testlog.txt` to locate all test logs in your project

The file contains detailed test execution results and is automatically generated by Meson's test runner.
