# Installation Guide

## Build and Install from Source

### Prerequisites

**Fedora 39+:**
```bash
sudo dnf install gcc-c++ meson ninja-build pkg-config \
  gtkmm4.0-devel protobuf-devel openssl-devel \
  libcorrect-devel gtest-devel desktop-file-utils appstream
```

**Note:** For FIPS-140-3 support, OpenSSL 3.5.0+ is required. See [FIPS Build](#fips-140-3-support) below.

**Ubuntu 24.04+:**
```bash
sudo apt-get install build-essential meson ninja-build pkg-config \
  libgtkmm-4.0-dev libprotobuf-dev protobuf-compiler \
  libssl-dev libgtest-dev cmake git desktop-file-utils appstream

# Build and install libcorrect (not in Ubuntu repos)
git clone https://github.com/quiet/libcorrect.git /tmp/libcorrect
cd /tmp/libcorrect
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
make -j$(nproc)
sudo make install
sudo ldconfig
```

**Note:** For FIPS-140-3 support, OpenSSL 3.5.0+ is required. See [FIPS Build](#fips-140-3-support) below.

### Build and Install

```bash
# Clone the repository
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower

# Configure with prefix (default: /usr/local, recommended: /usr)
meson setup build --prefix=/usr

# Build
meson compile -C build

# Run tests
meson test -C build

# Install (requires sudo for system-wide installation)
sudo meson install -C build
```

### Post-Installation

The installation automatically:
- Compiles GSettings schemas
- Updates icon cache
- Updates desktop database

If installing to a custom prefix, you may need to update these manually:

```bash
# For custom prefix installations
sudo glib-compile-schemas /usr/local/share/glib-2.0/schemas
sudo gtk4-update-icon-cache -q -t -f /usr/local/share/icons/hicolor
sudo update-desktop-database -q /usr/local/share/applications
```

### Verify Installation

```bash
# Check binary is installed
which keeptower

# Validate desktop file
desktop-file-validate /usr/share/applications/com.tjdeveng.keeptower.desktop

# Check icon installation
ls -l /usr/share/icons/hicolor/scalable/apps/com.tjdeveng.keeptower.svg

# Verify GSettings schema
gsettings list-schemas | grep com.tjdeveng.keeptower
```

### Launch Application

After installation, KeepTower will appear in your GNOME application grid:
- **Application Grid**: Search for "KeepTower" or "Password"
- **Command Line**: Run `keeptower`
- **Categories**: System → Security → KeepTower

The application icon should display correctly in:
- Application grid/launcher
- Window list/taskbar
- Alt+Tab switcher
- Header bar (symbolic icon)

### Uninstall

```bash
cd KeepTower
sudo ninja -C build uninstall
```

## Development Installation

For development, you can run directly from the build directory without installing:

```bash
# Set GSettings schema directory
export GSETTINGS_SCHEMA_DIR=$PWD/data

# Run from build directory
./build/src/keeptower
```

## Package Maintainers

### Files Installed

- **Binary**: `/usr/bin/keeptower`
- **Desktop Entry**: `/usr/share/applications/com.tjdeveng.keeptower.desktop`
- **Icons**:
  - `/usr/share/icons/hicolor/scalable/apps/com.tjdeveng.keeptower.svg`
  - `/usr/share/icons/hicolor/symbolic/apps/com.tjdeveng.keeptower-symbolic.svg`
- **AppStream Metadata**: `/usr/share/metainfo/com.tjdeveng.keeptower.appdata.xml`
- **GSettings Schema**: `/usr/share/glib-2.0/schemas/com.tjdeveng.keeptower.gschema.xml`

### Post-Install Scripts

Required for proper desktop integration:

```bash
glib-compile-schemas /usr/share/glib-2.0/schemas
gtk4-update-icon-cache -q -t -f /usr/share/icons/hicolor
update-desktop-database -q /usr/share/applications
```

These are automatically called by `meson install` unless `DESTDIR` is set (packaging).

### Dependencies

**Runtime:**
- GTKmm 4.0 >= 4.10
- OpenSSL >= 1.1.0
- Protocol Buffers >= 3.0
- libcorrect (Reed-Solomon error correction)

**Build:**
- C++23 compiler (GCC 13+ or Clang 16+)
- Meson >= 0.59.0
- Ninja
- pkg-config
- protoc (Protocol Buffers compiler)

**Optional:**
- GTest (for tests)
- Doxygen (for API documentation)
- desktop-file-utils (for validation)
- appstream (for metadata validation)

## Troubleshooting

### Application doesn't appear in menu

```bash
# Refresh desktop database
update-desktop-database ~/.local/share/applications
update-desktop-database /usr/share/applications

# Refresh icon cache
gtk4-update-icon-cache -f /usr/share/icons/hicolor

# Log out and back in, or restart GNOME Shell (Alt+F2, type 'r', Enter)
```

### Icon not displaying

Verify icon files are installed:
```bash
ls -l /usr/share/icons/hicolor/scalable/apps/com.tjdeveng.keeptower*.svg
```

If missing, reinstall:
```bash
sudo meson install -C build
```

### GSettings errors

Compile schemas manually:
```bash
sudo glib-compile-schemas /usr/share/glib-2.0/schemas
```

Check schema is installed:
```bash
gsettings list-schemas | grep keeptower
```

### Permission denied on run

Check file permissions:
```bash
ls -l /usr/bin/keeptower
# Should show: -rwxr-xr-x (executable)
```

## FIPS-140-3 Support

### Overview

KeepTower supports optional FIPS-140-3 compliant cryptographic operations using OpenSSL 3.5.0+ with the FIPS module. **This is optional and not required for normal use.**

**FIPS mode is recommended for:**
- Government and military organizations
- Financial institutions
- Healthcare providers (HIPAA compliance)
- Environments requiring cryptographic validation

### Quick FIPS Setup

**1. Build OpenSSL 3.5 with FIPS Module:**

KeepTower provides an automated script:

```bash
cd /path/to/KeepTower
./scripts/build-openssl-3.5.sh
```

This will:
- Download OpenSSL 3.5.0 source
- Compile with FIPS module enabled
- Run FIPS self-tests (35 Known Answer Tests)
- Install to `/usr/local/ssl`
- Generate FIPS configuration files

**Build time:** 5-10 minutes on modern hardware

**2. Configure Environment:**

Add to `~/.bashrc` or `~/.profile`:

```bash
export PKG_CONFIG_PATH=/usr/local/ssl/lib64/pkgconfig:$PKG_CONFIG_PATH
export LD_LIBRARY_PATH=/usr/local/ssl/lib64:$LD_LIBRARY_PATH
export OPENSSL_CONF=/usr/local/ssl/openssl.cnf
```

Then reload:
```bash
source ~/.bashrc
```

**3. Build KeepTower with FIPS Support:**

```bash
cd /path/to/KeepTower

# Configure build (will detect OpenSSL 3.5)
meson setup build --prefix=/usr

# Compile
meson compile -C build

# Run FIPS tests
meson test -C build "FIPS Mode Tests"

# Install
sudo meson install -C build
```

**4. Enable FIPS Mode in KeepTower:**

Launch KeepTower and:
1. Open **Preferences** (hamburger menu)
2. Go to **Security** tab
3. Check **"Enable FIPS-140-3 mode (requires restart)"**
4. Click **Apply**
5. **Restart KeepTower**

**5. Verify FIPS Mode:**

Open **Help → About KeepTower** and check for:
```
FIPS-140-3: Enabled ✓
```

### Comprehensive FIPS Documentation

For detailed FIPS setup, troubleshooting, and compliance information:

- **[FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md)** - Complete setup instructions
- **[FIPS_COMPLIANCE.md](FIPS_COMPLIANCE.md)** - Compliance documentation
- **[OPENSSL_35_MIGRATION.md](OPENSSL_35_MIGRATION.md)** - Technical details

### FIPS Build Verification

Verify FIPS support after building:

```bash
# Check OpenSSL version
openssl version
# Should show: OpenSSL 3.5.0 or higher

# Check FIPS provider is available
openssl list -providers
# Should list both 'base' and 'fips' providers

# Run FIPS tests
meson test -C build "FIPS Mode Tests"
# Should show: Ok: 11
```

### FIPS Mode Status

Check FIPS status:

```bash
# Check if FIPS mode is enabled
gsettings get com.tjdeveng.keeptower fips-mode-enabled

# Check runtime status in application logs
keeptower 2>&1 | grep -i fips
```

### Disabling FIPS Mode

To disable FIPS mode:

1. Open **Preferences → Security**
2. Uncheck **"Enable FIPS-140-3 mode"**
3. Click **Apply**
4. **Restart KeepTower**

Or via command line:
```bash
gsettings set com.tjdeveng.keeptower fips-mode-enabled false
```

### FIPS Troubleshooting

**"FIPS module not available" in Preferences:**

1. Verify OpenSSL 3.5 installed:
   ```bash
   openssl version
   ```

2. Check FIPS provider:
   ```bash
   openssl list -providers
   ```

3. Verify environment variables:
   ```bash
   echo $OPENSSL_CONF
   echo $LD_LIBRARY_PATH
   ```

4. See [FIPS_SETUP_GUIDE.md](FIPS_SETUP_GUIDE.md) for detailed troubleshooting

## Support

For issues, please report on GitHub:
https://github.com/tjdeveng/KeepTower/issues
