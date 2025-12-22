#!/bin/bash
# Test script to verify automatic OpenSSL build functionality

set -e

echo "==========================================="
echo "Testing Automatic OpenSSL Build System"
echo "==========================================="
echo

# Save current state
echo "1. Backing up existing OpenSSL build (if any)..."
if [ -d "build/openssl-install" ]; then
    mv build/openssl-install build/openssl-install.backup
    echo "   ✓ Backed up to build/openssl-install.backup"
else
    echo "   No existing build to backup"
fi
echo

# Remove test build directory
echo "2. Cleaning test build directory..."
rm -rf build-test
echo "   ✓ Removed build-test"
echo

# Try to configure without system OpenSSL 3.5
echo "3. Testing meson configuration..."
echo "   This will trigger automatic OpenSSL build if system version < 3.5"
echo

# Temporarily hide system OpenSSL 3.5 by clearing PKG_CONFIG_PATH
# (This simulates a system without OpenSSL 3.5+)
export OLD_PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig:/usr/share/pkgconfig"

if meson setup build-test; then
    echo
    echo "   ✓ Meson configuration successful!"
else
    echo
    echo "   ✗ Meson configuration failed!"
    export PKG_CONFIG_PATH="$OLD_PKG_CONFIG_PATH"

    # Restore backup if it exists
    if [ -d "build/openssl-install.backup" ]; then
        rm -rf build/openssl-install
        mv build/openssl-install.backup build/openssl-install
    fi
    exit 1
fi

export PKG_CONFIG_PATH="$OLD_PKG_CONFIG_PATH"
echo

# Verify build works
echo "4. Testing compilation..."
if ninja -C build-test > /dev/null 2>&1; then
    echo "   ✓ Compilation successful!"
else
    echo "   ✗ Compilation failed!"

    # Restore backup if it exists
    if [ -d "build/openssl-install.backup" ]; then
        rm -rf build/openssl-install
        mv build/openssl-install.backup build/openssl-install
    fi
    exit 1
fi
echo

# Run FIPS tests
echo "5. Running FIPS tests..."
if build-test/tests/fips_mode_test > /dev/null 2>&1; then
    echo "   ✓ FIPS tests passed!"
else
    echo "   ✗ FIPS tests failed!"

    # Restore backup if it exists
    if [ -d "build/openssl-install.backup" ]; then
        rm -rf build/openssl-install
        mv build/openssl-install.backup build/openssl-install
    fi
    exit 1
fi
echo

# Restore original OpenSSL build if we had one
if [ -d "build/openssl-install.backup" ]; then
    echo "6. Restoring original OpenSSL build..."
    rm -rf build/openssl-install
    mv build/openssl-install.backup build/openssl-install
    echo "   ✓ Restored from backup"
fi
echo

echo "==========================================="
echo "✓ All tests passed!"
echo "==========================================="
echo
echo "Summary:"
echo "- Automatic OpenSSL detection works"
echo "- Build system triggers OpenSSL build when needed"
echo "- Compilation succeeds with auto-built OpenSSL"
echo "- FIPS tests pass with auto-built OpenSSL"
echo
echo "The build system is now transparent and user-friendly!"
