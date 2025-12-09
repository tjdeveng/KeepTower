# Installation

This guide covers installation of KeepTower on various Linux distributions.

## Prerequisites

KeepTower requires the following libraries:
- GTKmm 4.0 (GTK4 C++ bindings)
- Protocol Buffers (libprotobuf)
- OpenSSL 3.x
- libcorrect (Reed-Solomon error correction)
- GIO/GLib 2.68+
- Meson build system
- C++23 compatible compiler (GCC 13+ or Clang 16+)

---

## Fedora / RHEL / CentOS Stream

### Install Dependencies

```bash
sudo dnf install gtkmm4.0-devel protobuf-devel openssl-devel \
    libcorrect-devel meson ninja-build gcc-c++ git
```

### Build from Source

```bash
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower
meson setup build
meson compile -C build
```

### Install System-wide (Optional)

```bash
sudo meson install -C build
```

### Run without Installing

```bash
./build/src/keeptower
```

---

## Ubuntu / Debian

### Install Dependencies

```bash
sudo apt update
sudo apt install libgtkmm-4.0-dev libprotobuf-dev protobuf-compiler \
    libssl-dev libcorrect-dev meson ninja-build g++ git
```

**Note:** On older Ubuntu versions, you may need to add a PPA for GTK4:

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
```

### Build from Source

```bash
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower
meson setup build
meson compile -C build
./build/src/keeptower
```

---

## Arch Linux / Manjaro

### Install Dependencies

```bash
sudo pacman -S gtkmm-4.0 protobuf openssl libcorrect meson ninja gcc git
```

### Build from Source

```bash
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower
meson setup build
meson compile -C build
./build/src/keeptower
```

### AUR Package (Coming Soon)

```bash
# Will be available as keeptower-git
yay -S keeptower-git
```

---

## openSUSE

### Install Dependencies

```bash
sudo zypper install gtkmm4-devel protobuf-devel libopenssl-3-devel \
    libcorrect-devel meson ninja gcc-c++ git
```

### Build from Source

```bash
git clone https://github.com/tjdeveng/KeepTower.git
cd KeepTower
meson setup build
meson compile -C build
./build/src/keeptower
```

---

## Flatpak (Coming Soon)

Flatpak packaging is planned for wider distribution compatibility:

```bash
# Future installation method
flatpak install flathub com.tjdeveng.keeptower
flatpak run com.tjdeveng.keeptower
```

---

## Building with Custom Options

### Debug Build

```bash
meson setup build --buildtype=debug
meson compile -C build
```

### Release Build with Optimizations

```bash
meson setup build --buildtype=release
meson compile -C build
```

### Install to Custom Prefix

```bash
meson setup build --prefix=/usr/local
meson compile -C build
sudo meson install -C build
```

---

## Running Tests

After building, run the test suite to verify everything works:

```bash
meson test -C build
```

All tests should pass. If any fail, please report an issue on GitHub.

---

## Uninstalling

If you installed system-wide:

```bash
sudo ninja -C build uninstall
```

Or simply remove the build directory if you ran without installing:

```bash
rm -rf build
```

---

## Troubleshooting

### "Package 'gtkmm-4.0' not found"

Your distribution may not have GTK4 packages yet. You may need to:
1. Add a PPA/COPR repository
2. Build GTK4 from source
3. Wait for your distribution to package GTK4

### "libcorrect not found"

Some distributions don't package libcorrect. You can build it manually:

```bash
git clone https://github.com/quiet/libcorrect.git
cd libcorrect
mkdir build && cd build
cmake ..
make
sudo make install
sudo ldconfig
```

### Build Errors with GCC

Ensure you have GCC 11 or later:

```bash
gcc --version
```

If your version is older, install a newer compiler or use Clang.

---

## Next Steps

After installation, proceed to **[[Getting Started]]** to create your first vault.
