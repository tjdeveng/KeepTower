# Help System

KeepTower's offline-first help system with automatic wiki synchronization.

## Quick Start

### For Users

Access help from the application:
1. Open KeepTower
2. Click hamburger menu (☰)
3. Select **Help** submenu
4. Choose your topic

Help opens in your default web browser.

### For Developers

#### Update Documentation

1. Edit markdown files in `docs/user/`
2. Commit and push to main branch
3. **Automatic**:
   - HTML regenerated during build
   - Wiki synced via GitHub Actions

#### Generate Help Locally

```bash
./scripts/generate-help.sh
xdg-open resources/help/00-home.html
```

## Architecture

```
docs/user/ (source) → pandoc → HTML → Hybrid Storage
                               ↓
                    ┌──────────┴──────────┐
                    ↓                     ↓
            Filesystem              GResources
       (/usr/share/keeptower/help)  (embedded)
```

## Features

- ✅ **Offline-first**: Works without internet
- ✅ **Hybrid storage**: Filesystem + embedded fallback
- ✅ **Auto-sync**: Wiki updates on commit
- ✅ **Secure**: Path validation, URI checks, size limits
- ✅ **Responsive**: UHD display optimized (16px font)
- ✅ **Dark mode**: Automatic theme switching

## Documentation

See [docs/developer/HELP_SYSTEM.md](../developer/HELP_SYSTEM.md) for complete documentation.

## File Structure

```
docs/user/               # Source documentation (markdown)
scripts/generate-help.sh # HTML generation script
resources/help/          # Generated HTML + CSS + template
src/utils/helpers/       # HelpManager implementation
.github/workflows/       # Wiki sync automation
```

## Dependencies

- **pandoc** (≥2.11) - Markdown to HTML conversion
- **GTK4** - Browser launching
- **GitHub Actions** - Wiki synchronization

## License

GPL-3.0-or-later
