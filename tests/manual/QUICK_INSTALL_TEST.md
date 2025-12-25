# Quick Installation Test

## Test System Installation

```bash
# Build and install to system
sudo meson install -C build

# Verify files are installed
which keeptower
desktop-file-validate /usr/share/applications/com.tjdeveng.keeptower.desktop
ls -l /usr/share/icons/hicolor/scalable/apps/com.tjdeveng.keeptower.svg

# Check GSettings schema
gsettings list-schemas | grep keeptower

# Test run from command line
keeptower &

# Check in GNOME app grid
# Press Super key, search for "KeepTower" or "password"
```

## Verify Desktop Integration

- [ ] Application appears in GNOME app grid
- [ ] Icon displays correctly in launcher
- [ ] Application is in System → Security category
- [ ] Icon appears in window list/taskbar
- [ ] Icon shows in Alt+Tab switcher
- [ ] Symbolic icon in header bar
- [ ] Search works with keywords: password, vault, security

## Uninstall Test

```bash
# Uninstall
sudo ninja -C build uninstall

# Verify removal
which keeptower  # should return nothing
ls /usr/share/applications/com.tjdeveng.keeptower.desktop  # should not exist
```

## Clean Reinstall Test

```bash
# Full clean reinstall
sudo ninja -C build uninstall
meson setup build --reconfigure --wipe --prefix=/usr
meson compile -C build
meson test -C build
sudo meson install -C build

# Verify again
keeptower --version
```
