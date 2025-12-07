# Installation Guide

## Build and Install from Source

### Prerequisites

**Fedora 39+:**
```bash
sudo dnf install gcc-c++ meson ninja-build pkg-config \
  gtkmm4.0-devel protobuf-devel openssl-devel \
  libcorrect-devel gtest-devel desktop-file-utils appstream
```

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

## Support

For issues, please report on GitHub:
https://github.com/tjdeveng/KeepTower/issues
