# Export Format Support

KeepTower supports exporting password data to multiple formats for portability.

## Supported Formats

### 1. CSV (Comma-Separated Values) ✅ Tested
**File Extension**: `.csv`
**Status**: Fully functional and tested

**Format**:
```
Account Name,Username,Password,Email,Website,Notes
Example Account,user@example.com,password123,user@example.com,https://example.com,My notes
```

**Use Cases**:
- Simple data backup
- Import into spreadsheet applications
- Manual data review

### 2. KeePass 2.x XML ⚠️ Not Fully Tested
**File Extension**: `.xml`
**Status**: Format implemented, KeePass import compatibility unverified

**Format**: KeePass 2.x unencrypted XML export format
**Target Application**: KeePass 2.x password manager

**Fields Mapping**:
- Title → Account Name
- UserName → Username
- Password → Password
- URL → Website
- Notes → Combined Email + Notes

**Note**: This format follows the KeePass 2.x XML structure. While the format is correctly implemented, actual import into KeePass has not been tested.

### 3. 1Password 1PIF ⚠️ Not Fully Tested
**File Extension**: `.1pif`
**Status**: Format implemented, 1Password import compatibility unverified

**Format**: 1Password Interchange Format (JSON-based)
**Target Application**: 1Password password manager

**Fields Mapping**:
- title → Account Name
- username field → Username
- password field → Password
- URLs → Website
- notesPlain → Combined Email + Notes

**Note**: This format follows the 1PIF specification. While the format is correctly implemented, actual import into 1Password has not been tested.

## Security Warnings

⚠️ **IMPORTANT**: All export formats produce UNENCRYPTED files containing passwords in plaintext.

**Security Measures Implemented**:
- Files created with 0600 permissions (owner read/write only)
- fsync() called to ensure data integrity
- File handles properly closed with RAII

**User Responsibilities**:
- Delete exported files immediately after use
- Never store unencrypted exports on cloud storage
- Use secure file transfer methods
- Consider encrypting the exported file with GPG or similar

## Export Process

1. Navigate to: Menu → Export Accounts
2. Enter vault password (and YubiKey if configured)
3. Choose export format in file dialog:
   - CSV files (*.csv) - Fully supported
   - KeePass XML (*.xml) - Not fully tested
   - 1Password 1PIF (*.1pif) - Not fully tested
   - All files - Format detected from extension
4. Select destination and confirm
   - File extension automatically updates when you change format filter
   - You can manually change the extension if needed

## Format Selection

The export format is automatically detected from the file extension in the filename:
- `.csv` → CSV format (default)
- `.xml` → KeePass XML format
- `.1pif` → 1Password 1PIF format

**Tip**: When you select a different format filter in the file chooser, the filename extension automatically updates to match.

## Testing Status

| Format | Export | Import to KeepTower | Round-Trip | Import to Target App |
|--------|--------|---------------------|------------|----------------------|
| CSV | ✅ Working | ✅ Working | ✅ Tested | N/A |
| KeePass XML | ✅ Working | ✅ Working | ✅ Ready | ⚠️ Not tested |
| 1Password 1PIF | ✅ Working | ✅ Working | ✅ Ready | ⚠️ Not tested |

**Notes**:
- CSV: Full round-trip verified (export → import back → data intact)
- KeePass XML: Round-trip complete, can now test KeepTower export → KeePass import when convenient
- 1Password 1PIF: Round-trip complete, can now test KeepTower export → 1Password import when convenient
- All formats support the same fields: Account Name, Username, Password, Email, Website, Notes

## Future Enhancements

Potential additions:
- Bitwarden JSON export
- LastPass CSV format
- Encrypted export with passphrase
- Selective export (choose specific accounts)

## Technical Details

### XML Special Character Escaping
The following characters are properly escaped in KeePass XML:
- `<` → `&lt;`
- `>` → `&gt;`
- `&` → `&amp;`
- `"` → `&quot;`
- `'` → `&apos;`

### 1PIF JSON Structure
Each entry is a single JSON object followed by a separator line:
```
{"uuid":"...","category":"001","title":"...","secureContents":{...}}
***5642bee8-a5ff-11dc-8314-0800200c9a66***
```

## Code Location

Implementation files:
- `src/utils/ImportExport.{h,cc}` - Export functions
- `src/ui/windows/MainWindow.cc` - UI integration

## Feedback

If you test KeePass XML or 1Password 1PIF import functionality:
- Report success/failures via GitHub issues
- Include format version information
- Note any data loss or corruption

---

**Last Updated**: 2025-12-13
**Version**: 0.2.5-beta
