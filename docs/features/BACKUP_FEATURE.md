# Timestamped Backup Feature

## Overview

KeepTower now includes automatic timestamped backup creation to protect against accidental data loss. This complements the existing encryption and Reed-Solomon error correction features.

## Implementation

### Core Changes

**VaultManager (src/core/VaultManager.h/.cc)**
- `create_backup()`: Creates timestamped backups with format `vault.vault.backup.YYYYmmdd_HHMMSS_mmm`
- `cleanup_old_backups()`: Removes oldest backups when limit exceeded
- `list_backups()`: Returns sorted list of all backup files for a vault
- `restore_from_backup()`: Enhanced to restore from most recent timestamped backup
- Millisecond-precision timestamps prevent collisions during rapid saves
- Backwards compatible with legacy `.backup` format

### Configuration

**GSettings Schema (data/com.tjdeveng.keeptower.gschema.xml)**
- `backup-enabled` (bool): Enable/disable automatic backups (default: true)
- `backup-count` (int): Maximum backups to retain, range 1-50 (default: 5)

### User Interface

**PreferencesDialog (src/ui/dialogs/PreferencesDialog.h/.cc)**
- New "Automatic Backups" section added below Reed-Solomon section
- Enable/disable checkbox
- Backup count spinner with validation
- Clear descriptions explaining the feature
- Sensitivity management: controls disabled when backups off

**MainWindow (src/ui/windows/MainWindow.cc)**
- Loads backup settings from GSettings on startup
- Applies configuration to VaultManager instance

## User Experience

### Benefits
1. **Protection Against User Error**: Accidental deletions or corruptions recoverable
2. **Configurable Retention**: Users choose how many backups to keep (1-50)
3. **Automatic Cleanup**: Old backups deleted automatically, no manual maintenance
4. **Timestamped Names**: Easy identification of backup age
5. **Non-Intrusive**: Backup failure doesn't prevent vault save operation
6. **Default Enabled**: Protection active out-of-box for user confidence

### File Naming
```
vault.vault                              # Main vault file
vault.vault.backup.20251207_183045_123   # Backup from Dec 7, 2025, 18:30:45.123
vault.vault.backup.20251207_183046_456   # Backup from Dec 7, 2025, 18:30:46.456
```

### Backup Rotation Example
With `backup-count=5`:
1. User saves vault → 1st backup created
2. User saves again → 2nd backup created
3. Continue until 5 backups exist
4. 6th save → Oldest backup deleted, new backup created
5. Always maintains exactly 5 most recent backups

## Technical Details

### Timestamp Format
- Pattern: `YYYYmmdd_HHMMSS_mmm`
- Example: `20251207_183045_123` = December 7, 2025, 18:30:45.123
- Millisecond precision prevents collision during rapid saves
- Lexicographically sortable for easy newest/oldest determination

### Backup Lifecycle
1. **Creation**: On every vault save (if enabled)
2. **Naming**: Timestamp appended to vault path
3. **Cleanup**: Immediate after successful backup creation
4. **Sorting**: Descending order (newest first) for cleanup
5. **Restoration**: Automatic selection of most recent backup

### Error Handling
- Backup creation failure logged as warning, doesn't abort save
- Cleanup failures logged but don't prevent new backups
- Missing backup directory handled gracefully
- Legacy backup format detected and used if available

### Integration Points
- `save_vault()`: Creates backup before writing, cleans up after
- Atomic save operations: Backup→Save→Cleanup sequence
- Reed-Solomon: Works independently, both features can be enabled
- GSettings: Preferences persist across application restarts

## Testing

### Test Coverage
- All 103 existing tests continue passing
- Timestamp collision prevention verified (millisecond precision)
- Backwards compatibility with legacy backups confirmed
- VaultManager test suite includes backup scenarios

### Manual Testing Guide
1. Run: `GSETTINGS_SCHEMA_DIR=$PWD/data ./build/src/keeptower`
2. Create vault and add accounts
3. Save multiple times
4. Verify backups: `ls -lh *.vault.backup.*`
5. Open Preferences → Automatic Backups
6. Change backup count to 3
7. Save several more times
8. Verify only 3 backups remain

## Security Considerations

- Backup files inherit restrictive permissions from main vault
- No additional encryption applied (copies are encrypted like source)
- Backup cleanup uses secure filesystem operations
- No sensitive data logged during backup operations
- Memory-safe C++23 code with bounds checking

## Future Enhancements (Optional)

1. **Restore UI**: Dialog to select and restore specific backup
2. **Backup Compression**: Optional gzip compression for old backups
3. **Backup Location**: Configure separate backup directory
4. **Backup Schedule**: Time-based instead of save-based backups
5. **Backup Verification**: Integrity check on backup files

## Commit History

- Initial implementation: feat: add timestamped backups with configurable retention
- 8 files modified, 302 lines added
- Fully integrated with existing preference system
- Documentation updated in README.md

## Conclusion

The timestamped backup feature provides critical protection against accidental data loss while maintaining KeepTower's focus on security and user experience. Combined with AES-256-GCM encryption and Reed-Solomon error correction, users now have comprehensive protection for their password vault against corruption, hardware failure, and human error.
