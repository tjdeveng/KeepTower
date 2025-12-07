# Reed-Solomon Preferences Feature - Testing Guide

## Overview

KeepTower now includes a Preferences dialog that allows users to configure Reed-Solomon (RS) error correction for vault files. This feature provides protection against data corruption, bit rot, and disk errors.

## Features Implemented

### 1. PreferencesDialog UI (`src/ui/dialogs/PreferencesDialog.h/cc`)
- Modal dialog with clean, simple interface
- Checkbox to enable/disable Reed-Solomon error correction
- Spin button for redundancy level (5-50%)
- Helpful description text explaining the feature
- GSettings integration for persistent settings

### 2. MainWindow Integration (`src/ui/windows/MainWindow.h/cc`)
- New "Preferences" button in toolbar with gear icon
- Signal handler `on_preferences()` to show dialog
- Settings loaded from GSettings on startup
- Settings applied to VaultManager automatically

### 3. GSettings Schema (`data/com.tjdeveng.keeptower.gschema.xml`)
- `use-reed-solomon` (boolean, default: false)
- `rs-redundancy-percent` (integer, range: 5-50, default: 10)

### 4. Documentation Updates
- README.md: Added FEC feature description, usage instructions
- SECURITY.md: Added error correction section
- test_preferences.sh: Helper script to test GSettings

## How to Test

### Build and Run

```bash
# Build
ninja -C build

# Run all tests (should show 103 passing)
ninja -C build test

# Run with local GSettings schema
GSETTINGS_SCHEMA_DIR=$PWD/data ./build/src/keeptower
```

### Test Scenarios

#### Scenario 1: Basic Preferences UI
1. Launch KeepTower
2. Click **Preferences** button (gear icon) in toolbar
3. Verify dialog opens with:
   - "Enable error correction for new vaults" checkbox (unchecked by default)
   - Redundancy spin button showing 10% (disabled)
   - Help text explaining higher values increase protection and file size
4. Check the **Enable** checkbox
5. Verify redundancy spin button becomes enabled
6. Adjust redundancy slider to 20%
7. Click **OK**
8. Re-open Preferences and verify settings persisted (checked, 20%)

#### Scenario 2: Creating Vaults with RS Enabled
1. Open Preferences, enable RS with 20% redundancy, click OK
2. Click **New Vault**
3. Choose location, enter master password
4. Add some test accounts
5. Save vault
6. Close vault
7. Check vault file size (should be ~20% larger than without RS)
8. Re-open vault
9. Verify all data intact

#### Scenario 3: Backward Compatibility
1. Open Preferences, **disable** RS, click OK
2. Create a new vault (will not use RS)
3. Add accounts and save
4. Enable RS in Preferences
5. Open the old vault (created without RS)
6. Verify it still opens correctly
7. Make changes and save
8. **Existing vault remains without RS** (RS only applies to new vaults)

#### Scenario 4: GSettings Command Line
```bash
# View current settings
gsettings --schemadir=data get com.tjdeveng.keeptower use-reed-solomon
gsettings --schemadir=data get com.tjdeveng.keeptower rs-redundancy-percent

# Enable RS with 30% redundancy
gsettings --schemadir=data set com.tjdeveng.keeptower use-reed-solomon true
gsettings --schemadir=data set com.tjdeveng.keeptower rs-redundancy-percent 30

# Launch app - settings should be reflected in Preferences dialog
GSETTINGS_SCHEMA_DIR=$PWD/data ./build/src/keeptower
```

#### Scenario 5: Corruption Recovery (Advanced)
1. Enable RS with 30% redundancy in Preferences
2. Create a vault and add test data
3. Close KeepTower
4. Use hex editor to corrupt a few bytes in middle of vault file
5. Re-open vault
6. If corruption is within recovery threshold (15% for 30% redundancy), vault should open successfully
7. If too much corruption, error message will indicate corruption

## Expected Behavior

### Settings Persistence
- Settings saved to GSettings when clicking **OK** in Preferences
- Settings loaded from GSettings when KeepTower starts
- Settings applied to VaultManager automatically
- Changes take effect for **new vaults only**

### Vault File Format
- Vaults created with RS enabled include:
  - FLAG_RS_ENABLED (bit 0 set in flags byte)
  - Redundancy percentage stored
  - Original data size stored
  - RS-encoded data with parity blocks
- Vaults created without RS:
  - Traditional format (backward compatible)
  - No flags, no redundancy data

### UI Responsiveness
- Redundancy spin button disabled when RS checkbox unchecked
- Spin button enabled when checkbox checked
- Settings persist across app restarts
- No effect on already-open vaults

## Troubleshooting

### GSettings Schema Not Found Error
```
GLib-GIO-ERROR **: Settings schema 'com.tjdeveng.keeptower' is not installed
```

**Solution**: Run with schema directory specified:
```bash
cd /home/tjdev/Projects/KeepTower
glib-compile-schemas data/
GSETTINGS_SCHEMA_DIR=$PWD/data ./build/src/keeptower
```

### Preferences Button Not Visible
- Verify `m_preferences_button` added to MainWindow.h
- Verify button appended to toolbar in MainWindow.cc constructor
- Check build output for compilation errors

### Settings Not Persisting
- Check GSettings schema is compiled: `ls data/gschemas.compiled`
- Verify `GSETTINGS_SCHEMA_DIR` environment variable set correctly
- Use `gsettings monitor com.tjdeveng.keeptower` to watch for changes

## Technical Details

### Architecture
```
User clicks Preferences
    ↓
MainWindow::on_preferences()
    ↓
PreferencesDialog shown
    ↓
User changes settings, clicks OK
    ↓
PreferencesDialog::on_response()
    ↓
Settings saved to GSettings
    ↓
(Next time app starts)
    ↓
MainWindow constructor loads settings
    ↓
VaultManager::set_reed_solomon_enabled()
VaultManager::set_rs_redundancy_percent()
    ↓
Settings applied to new vaults
```

### Code Locations
- **Dialog UI**: `src/ui/dialogs/PreferencesDialog.h`, `PreferencesDialog.cc`
- **Integration**: `src/ui/windows/MainWindow.h` (line 74, 99), `MainWindow.cc` (lines 23, 46-51, 155, 175-177, 794-797)
- **Settings Schema**: `data/com.tjdeveng.keeptower.gschema.xml`
- **RS Implementation**: `src/core/ReedSolomon.h`, `ReedSolomon.cc`
- **Tests**: `tests/test_reed_solomon.cc`, `tests/test_vault_reed_solomon.cc`

## Test Results

All 103 tests passing:
- ✅ 3 validation tests (desktop, appdata, schema)
- ✅ 8 input validation tests
- ✅ 13 Reed-Solomon unit tests
- ✅ 21 password validation tests
- ✅ 8 RS integration tests (corruption recovery)
- ✅ 50 VaultManager tests

Build: Clean, no warnings or errors.

## Next Steps

1. **Install to system** (requires sudo):
   ```bash
   sudo ninja -C build install
   ```

2. **Create release package**:
   ```bash
   git tag v0.1.1-beta
   git push origin v0.1.1-beta
   ```

3. **User testing**:
   - Test on clean system
   - Verify GSettings schema installed correctly
   - Test with various redundancy levels
   - Benchmark file size increase vs recovery capability

4. **Future enhancements**:
   - Add RS status indicator in UI showing if current vault uses RS
   - Add "Convert to RS" option for existing vaults
   - Add RS statistics (bytes protected, recovery threshold)
   - Add test mode to intentionally corrupt and recover test vault
