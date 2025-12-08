# Flatpak Testing Guide

This document explains how to build and test KeepTower as a Flatpak locally before submitting to Flathub.

## Prerequisites

### Install Flatpak and flatpak-builder

**Fedora:**
```bash
sudo dnf install flatpak flatpak-builder
```

**Ubuntu:**
```bash
sudo apt install flatpak flatpak-builder
```

### Add Flathub repository (if not already added)
```bash
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
```

### Install GNOME SDK (required for building)
```bash
flatpak install flathub org.gnome.Platform//47 org.gnome.Sdk//47
```

## Local Testing

### 1. Build and Install Locally

From the KeepTower project directory:

```bash
# Clean previous builds (optional)
rm -rf build-flatpak

# Build and install to user's local Flatpak repo
flatpak-builder --user --install --force-clean build-flatpak com.tjdeveng.KeepTower.yml
```

This will:
- Download and build libcorrect
- Build KeepTower from your local source
- Install it to your user Flatpak repository

### 2. Run the Flatpak

```bash
flatpak run com.tjdeveng.KeepTower
```

### 3. Test Functionality

- Create a new vault
- Add/edit/delete password entries
- Test search and clipboard functions
- Verify file permissions (vaults should save to home directory)
- Check appearance preferences
- Test Reed-Solomon features

### 4. Check Logs

If something goes wrong:
```bash
flatpak run --command=sh com.tjdeveng.KeepTower
# Now you're inside the sandbox
keeptower
```

Or view system logs:
```bash
journalctl --user -f | grep keeptower
```

## Development Workflow

### Quick Rebuild After Code Changes

```bash
# Rebuild and reinstall
flatpak-builder --user --install --force-clean build-flatpak com.tjdeveng.KeepTower.yml

# Run updated version
flatpak run com.tjdeveng.KeepTower
```

### Test Sandbox Permissions

Check what files the app can access:
```bash
flatpak run --command=bash com.tjdeveng.KeepTower
ls -la ~/
# Should see your home directory files
```

### Override Permissions (for testing)

Grant additional access temporarily:
```bash
flatpak override --user --filesystem=/tmp com.tjdeveng.KeepTower
```

Reset to defaults:
```bash
flatpak override --user --reset com.tjdeveng.KeepTower
```

## Cleaning Up

### Uninstall the Flatpak
```bash
flatpak uninstall --user com.tjdeveng.KeepTower
```

### Remove build directory
```bash
rm -rf build-flatpak
```

### Clear Flatpak cache (optional)
```bash
flatpak uninstall --unused
```

## Preparing for Flathub Submission

When ready to submit to Flathub:

### 1. Switch from local directory to git source

Edit `com.tjdeveng.KeepTower.yml`:

```yaml
sources:
  - type: git
    url: https://github.com/tjdeveng/KeepTower.git
    tag: v0.2.0-rc1
    commit: <full-git-commit-hash>
```

### 2. Test with git source

```bash
flatpak-builder --user --install --force-clean build-flatpak com.tjdeveng.KeepTower.yml
```

### 3. Run Flathub validation

```bash
flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest com.tjdeveng.KeepTower.yml
flatpak run --command=flatpak-builder-lint org.flatpak.Builder builddir build-flatpak
```

### 4. Create Flathub repository

- Fork https://github.com/flathub/flathub
- Add your manifest to the fork
- Submit PR to Flathub

## Common Issues

### Issue: "Failed to init: Unable to find runtime"
**Solution:** Install the GNOME runtime:
```bash
flatpak install flathub org.gnome.Platform//47 org.gnome.Sdk//47
```

### Issue: Build fails with missing dependencies
**Solution:** Check that all modules in the manifest are correctly specified

### Issue: App can't access vault files
**Solution:** Check finish-args for filesystem permissions in manifest

### Issue: Icons don't show up
**Solution:** Verify icon installation paths match Flatpak's /app prefix

## Resources

- [Flatpak Documentation](https://docs.flatpak.org/)
- [Flathub Submission Guide](https://github.com/flathub/flathub/wiki/App-Submission)
- [GNOME Application Guidelines](https://developer.gnome.org/documentation/guidelines.html)
- [Flatpak Builder Manifest Format](https://docs.flatpak.org/en/latest/flatpak-builder-command-reference.html)
