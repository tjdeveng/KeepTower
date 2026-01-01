# Help System Documentation

## Overview

KeepTower implements an offline-first help system with automatic wiki synchronization. This document describes the architecture, implementation, and maintenance of the help system.

## Architecture

### Single Source of Truth

```
Main Repository (docs/user/)
    ↓ (build time)
HTML Generation (pandoc)
    ↓
Hybrid Distribution
    ├─→ Filesystem (/usr/share/keeptower/help/)
    └─→ Embedded (GResources)

Main Repository (docs/user/)
    ↓ (on commit)
GitHub Actions (sync-wiki.yml)
    ↓
GitHub Wiki
```

### Design Principles

1. **Single Source**: All documentation originates in `docs/user/`
2. **Offline-First**: Help works without internet connection
3. **Hybrid Storage**: Filesystem + embedded GResources fallback
4. **Automatic Sync**: Wiki auto-updates on commit

## Components

### 1. Source Documentation

**Location**: `docs/user/`

```
docs/user/
├── 00-home.md              → Home.md (wiki)
├── 01-getting-started.md   → Getting-Started.md (wiki)
├── 02-installation.md      → Installation.md (wiki)
├── 03-user-guide.md        → User-Guide.md (wiki)
├── 04-faq.md               → FAQ.md (wiki)
├── 05-security.md          → Security.md (wiki)
└── SECURITY_BEST_PRACTICES.md
```

**Format**: Standard Markdown with GitHub Flavored Markdown extensions

### 2. HTML Generation

**Script**: `scripts/generate-help.sh`

**Process**:
1. Reads markdown from `docs/user/`
2. Converts to HTML5 using pandoc
3. Applies custom template and CSS
4. Outputs to `resources/help/`

**Build Integration**: Runs automatically during `meson compile`

**Template**: `resources/help/template.html`
- Navigation bar with all help pages
- Responsive design
- Dark mode support

**Styling**: `resources/help/css/help-style.css`
- Base font size: 16px (optimized for UHD displays)
- Color scheme: Light/dark mode support
- Typography: Cantarell, DejaVu Sans

### 3. Help Manager

**Files**:
- `src/utils/helpers/HelpManager.h`
- `src/utils/helpers/HelpManager.cc`

**Features**:
- Singleton pattern for global access
- File discovery with 3-tier fallback
- Security validation (path traversal, URI schemes, file sizes)
- GTK4 integration for browser launching

**File Discovery Strategy**:
1. **Installed location**: `$DATADIR/keeptower/help/` (production)
2. **Development paths**: Relative to executable (4 fallback locations)
3. **GResources**: Embedded in binary, extracted to temp files

**Security**:
- Path traversal prevention (validates `..`, `/`, `\`)
- URI scheme validation (only `file://`)
- File size limits (10MB max)
- Canonical path resolution

### 4. Menu Integration

**File**: `src/ui/managers/MenuManager.cc`

**Menu Structure**:
```
☰ Hamburger Menu
  └── Help ▶
      ├── User Guide
      ├── Getting Started
      ├── FAQ
      └── Security
```

**Actions**: `win.help-user-guide`, `win.help-getting-started`, `win.help-faq`, `win.help-security`

### 5. Build System

**Resources**: `resources/help.gresource.xml`
- Embeds all HTML files and CSS
- Compressed for size optimization
- Total size: ~244KB embedded in binary

**Installation**: `meson.build`
- Installs help files to `$datadir/keeptower/help/`
- Handles missing pandoc gracefully
- Both filesystem and embedded available

### 6. CI/CD Automation

**Workflow**: `.github/workflows/sync-wiki.yml`

**Triggers**:
- Push to `main` branch with changes in `docs/user/`
- Push to `main` branch with changes in `docs/developer/CONTRIBUTING.md`
- Manual dispatch via GitHub Actions UI

**Process**:
1. Checkout main repository
2. Checkout wiki repository
3. Copy and rename documentation files
4. Add sync timestamp
5. Commit and push to wiki

**Security**: Uses `GITHUB_TOKEN` with `contents: write` permission

## Usage

### For Users

Help is accessible from the application:
1. Click hamburger menu (☰)
2. Select **Help** submenu
3. Choose desired topic

Help opens in default web browser.

### For Developers

#### Adding New Help Topics

1. **Create markdown file**: `docs/user/06-new-topic.md`
2. **Update HelpManager.h**:
   ```cpp
   enum class HelpTopic {
       // ... existing topics ...
       NewTopic
   };
   ```
3. **Update HelpManager.cc**:
   ```cpp
   const std::map<HelpTopic, std::string> HelpManager::topic_filenames_ = {
       // ... existing mappings ...
       {HelpTopic::NewTopic, "06-new-topic.html"}
   };
   ```
4. **Update MenuManager.cc**: Add menu item
5. **Update sync-wiki.yml**: Add file copy step
6. **Rebuild**: `meson compile -C build`

#### Updating Existing Documentation

1. Edit markdown file in `docs/user/`
2. Commit changes to main branch
3. **Automatic**:
   - Help HTML regenerated during next build
   - Wiki synced via GitHub Actions
   - Users get updated help on next install

#### Testing Help Locally

```bash
# Generate HTML
./scripts/generate-help.sh

# View in browser
xdg-open resources/help/00-home.html

# Test in application
./build/src/keeptower
# Navigate: ☰ → Help → User Guide
```

#### Updating CSS/Styling

Edit `resources/help/css/help-style.css`, then:

```bash
./scripts/generate-help.sh
xdg-open resources/help/00-home.html
```

## Maintenance

### Regular Tasks

- **Review documentation**: Ensure accuracy with each release
- **Update screenshots**: Keep UI references current
- **Check wiki sync**: Verify GitHub Actions success
- **Test help system**: Ensure all links work

### Troubleshooting

#### Help not opening

1. Check file permissions: `ls -l /usr/share/keeptower/help/`
2. Check GResources: `strings build/src/keeptower | grep keeptower_help`
3. Check logs: Application should show error dialog

#### Wiki not syncing

1. Check workflow runs: GitHub → Actions → Sync Documentation to Wiki
2. Check permissions: Repository settings → Actions → Workflow permissions
3. Manually trigger: Actions → Sync Documentation to Wiki → Run workflow

#### HTML generation fails

1. Check pandoc: `pandoc --version` (requires 2.11+)
2. Check markdown: Validate with `pandoc -f markdown -t html file.md`
3. Check template: Verify `resources/help/template.html` exists

### Performance

- **HTML size**: 14-37KB per page (compressed in GResources)
- **Build time**: ~2 seconds for HTML generation
- **Runtime overhead**: Negligible (lazy loading)

## Best Practices

### Documentation Writing

1. **Use clear headings**: Organize with h2-h4
2. **Add code examples**: Use fenced code blocks
3. **Include screenshots**: Reference UI elements
4. **Cross-link**: Reference related topics
5. **Keep concise**: Focus on user tasks

### Markdown Guidelines

- Use GitHub Flavored Markdown
- Avoid HTML (pandoc converts)
- Use relative links for cross-references
- Add alt text to images
- Use tables for structured data

### Security

- **Never embed**: Credentials, API keys, personal data
- **Validate paths**: HelpManager handles this automatically
- **URI schemes**: Only `file://` allowed
- **File sizes**: 10MB limit enforced

## Future Enhancements

### Potential Improvements

1. **Search functionality**: Full-text search within help
2. **Keyboard navigation**: Arrow keys between pages
3. **Print support**: CSS print media queries
4. **Translations**: Multi-language support via gettext
5. **Context-sensitive help**: F1 key for current screen help
6. **Offline full-text search**: Embed search index

### Migration Path

If moving away from wiki:
1. Keep `docs/user/` as source
2. Update sync workflow target
3. Regenerate help HTML
4. No application changes needed

## Related Files

### Source Files
- `docs/user/` - Source documentation
- `src/utils/helpers/HelpManager.{h,cc}` - Help system logic
- `src/ui/managers/MenuManager.{h,cc}` - Menu integration

### Build Files
- `scripts/generate-help.sh` - HTML generation script
- `resources/help.gresource.xml` - GResources manifest
- `resources/help/template.html` - HTML template
- `resources/help/css/help-style.css` - Stylesheet
- `meson.build` - Installation rules

### CI/CD Files
- `.github/workflows/sync-wiki.yml` - Wiki sync workflow

## License

All documentation follows the same GPL-3.0-or-later license as KeepTower.

## Authors

- tjdeveng - Initial implementation (2026)

## Changelog

### 2026-01-01 - Initial Implementation
- Created offline-first help system
- Implemented hybrid storage (filesystem + GResources)
- Added pandoc-based HTML generation
- Created wiki sync automation
- Implemented HelpManager with security features
- Added Help submenu to hamburger menu
- Optimized font size for UHD displays (16px)
