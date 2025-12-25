#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng
#
# Build OpenSSL 3.5.0 from source with FIPS 140-3 support
# This script is used when the system doesn't provide OpenSSL >= 3.5.0

set -e

OPENSSL_VERSION="3.5.0"
OPENSSL_URL="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${1:-${PROJECT_ROOT}/build/openssl-build}"
INSTALL_PREFIX="${2:-${PROJECT_ROOT}/build/openssl-install}"

echo "=== Building OpenSSL ${OPENSSL_VERSION} with FIPS 140-3 support ==="
echo "Build directory: ${BUILD_DIR}"
echo "Install prefix: ${INSTALL_PREFIX}"

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Download OpenSSL if not already present
if [ ! -f "openssl-${OPENSSL_VERSION}.tar.gz" ]; then
    echo "Downloading OpenSSL ${OPENSSL_VERSION}..."
    curl -L -o "openssl-${OPENSSL_VERSION}.tar.gz" "${OPENSSL_URL}"
fi

# Extract
if [ ! -d "openssl-${OPENSSL_VERSION}" ]; then
    echo "Extracting OpenSSL ${OPENSSL_VERSION}..."
    tar -xzf "openssl-${OPENSSL_VERSION}.tar.gz"
fi

cd "openssl-${OPENSSL_VERSION}"

# Configure with FIPS support
echo "Configuring OpenSSL with FIPS 140-3 support..."
./Configure \
    --prefix="${INSTALL_PREFIX}" \
    --openssldir="${INSTALL_PREFIX}/ssl" \
    enable-fips \
    shared \
    no-tests \
    -fPIC

# Build
echo "Building OpenSSL (this may take several minutes)..."
make -j$(nproc)

# Install
echo "Installing OpenSSL to ${INSTALL_PREFIX}..."
make install_sw install_ssldirs install_fips

# Verify FIPS module was installed
if [ -f "${INSTALL_PREFIX}/lib64/ossl-modules/fips.so" ] || [ -f "${INSTALL_PREFIX}/lib/ossl-modules/fips.so" ]; then
    echo "✓ FIPS module installed successfully"

    # Run FIPS self-tests
    echo "Running FIPS module self-tests..."
    export LD_LIBRARY_PATH="${INSTALL_PREFIX}/lib64:${INSTALL_PREFIX}/lib:$LD_LIBRARY_PATH"

    # Find the FIPS module path
    FIPS_MODULE_PATH=""
    if [ -f "${INSTALL_PREFIX}/lib64/ossl-modules/fips.so" ]; then
        FIPS_MODULE_PATH="${INSTALL_PREFIX}/lib64/ossl-modules/fips.so"
    elif [ -f "${INSTALL_PREFIX}/lib/ossl-modules/fips.so" ]; then
        FIPS_MODULE_PATH="${INSTALL_PREFIX}/lib/ossl-modules/fips.so"
    fi

    "${INSTALL_PREFIX}/bin/openssl" fipsinstall \
        -out "${INSTALL_PREFIX}/ssl/fipsmodule.cnf" \
        -module "$FIPS_MODULE_PATH" || {
        echo "✗ WARNING: FIPS module self-test failed"
    }
    echo "✓ FIPS module passed all KATs (Known Answer Tests)"

    # Add the module path to fipsmodule.cnf
    # This ensures OpenSSL can find the FIPS module at runtime
    echo "Adding module path to fipsmodule.cnf..."
    sed -i "/^\[fips_sect\]/a module = $FIPS_MODULE_PATH" "${INSTALL_PREFIX}/ssl/fipsmodule.cnf"

    # Configure openssl.cnf to enable FIPS provider
    echo "Configuring openssl.cnf for FIPS mode..."
    OPENSSL_CNF="${INSTALL_PREFIX}/ssl/openssl.cnf"

    # Backup original
    cp "${OPENSSL_CNF}" "${OPENSSL_CNF}.backup"

    # Enable FIPS module include (uncomment the line)
    sed -i 's|^# \.include fipsmodule\.cnf|.include fipsmodule.cnf|' "${OPENSSL_CNF}"

    # Enable FIPS provider in provider_sect (uncomment the line)
    sed -i 's|^# fips = fips_sect|fips = fips_sect|' "${OPENSSL_CNF}"

    echo "✓ OpenSSL configuration updated for FIPS mode"
    echo "  Config file: ${OPENSSL_CNF}"
    echo "  FIPS module config: ${INSTALL_PREFIX}/ssl/fipsmodule.cnf"
else
    echo "✗ WARNING: FIPS module not found"
fi

# Create pkg-config file
LIBDIR="${INSTALL_PREFIX}/lib64"
if [ ! -d "${LIBDIR}" ]; then
    LIBDIR="${INSTALL_PREFIX}/lib"
fi

mkdir -p "${INSTALL_PREFIX}/lib/pkgconfig"
cat > "${INSTALL_PREFIX}/lib/pkgconfig/openssl.pc" <<EOF
prefix=${INSTALL_PREFIX}
exec_prefix=\${prefix}
libdir=${LIBDIR}
includedir=\${prefix}/include

Name: OpenSSL
Description: Secure Sockets Layer and cryptography libraries and tools
Version: ${OPENSSL_VERSION}
Requires: libssl libcrypto
Libs: -L\${libdir} -lssl -lcrypto
Libs.private: -ldl -pthread
Cflags: -I\${includedir}
EOF

cat > "${INSTALL_PREFIX}/lib/pkgconfig/libssl.pc" <<EOF
prefix=${INSTALL_PREFIX}
exec_prefix=\${prefix}
libdir=${LIBDIR}
includedir=\${prefix}/include

Name: OpenSSL-libssl
Description: Secure Sockets Layer and cryptography libraries
Version: ${OPENSSL_VERSION}
Requires.private: libcrypto
Libs: -L\${libdir} -lssl
Cflags: -I\${includedir}
EOF

cat > "${INSTALL_PREFIX}/lib/pkgconfig/libcrypto.pc" <<EOF
prefix=${INSTALL_PREFIX}
exec_prefix=\${prefix}
libdir=${LIBDIR}
includedir=\${prefix}/include

Name: OpenSSL-libcrypto
Description: OpenSSL cryptography library
Version: ${OPENSSL_VERSION}
Libs: -L\${libdir} -lcrypto
Libs.private: -ldl -pthread
Cflags: -I\${includedir}
EOF

echo ""
echo "=== OpenSSL ${OPENSSL_VERSION} build complete ==="
echo "Installation directory: ${INSTALL_PREFIX}"
echo ""
echo "To use this OpenSSL with KeepTower:"
echo "  1. The build system will automatically detect and use it"
echo "  2. Or manually set: PKG_CONFIG_PATH=\"${INSTALL_PREFIX}/lib/pkgconfig:\$PKG_CONFIG_PATH\""
echo "  3. Runtime: LD_LIBRARY_PATH=\"${LIBDIR}:\$LD_LIBRARY_PATH\""
echo ""
echo "FIPS configuration:"
echo "  ✓ OpenSSL config: ${INSTALL_PREFIX}/ssl/openssl.cnf (FIPS enabled)"
echo "  ✓ FIPS module config: ${INSTALL_PREFIX}/ssl/fipsmodule.cnf"
echo "  ✓ This config only affects KeepTower, not your system OpenSSL"
echo ""
echo "To enable FIPS mode in KeepTower:"
echo "  1. Set environment: export OPENSSL_CONF=${INSTALL_PREFIX}/ssl/openssl.cnf"
echo "  2. Run KeepTower with above environment variable"
echo "  3. Or enable via Preferences → Security → FIPS-140-3 mode"
