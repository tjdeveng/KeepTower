# KeepTower Scripts

This directory contains utility scripts for KeepTower development and maintenance.

## Available Scripts

### `cleanup.sh`

Removes KeepTower configuration and cache files. Safe to run - **never touches vault files or backups**.

**Usage:**
```bash
./scripts/cleanup.sh
```

**What it removes:**
- GSettings configuration (`com.tjdeveng.keeptower`)
- Compiled GSchema cache (`~/.local/share/glib-2.0/schemas/gschemas.compiled`)
- Application config directory (`~/.config/keeptower/`)
- dconf entries (if dconf is available)

**What it preserves:**
- ✓ Vault files (`*.vault`)
- ✓ Vault backups (`*.vault.backup`)
- ✓ All user data

**When to use:**
- After uninstalling for complete removal
- Before fresh reinstall
- Fixing corrupted settings or schema conflicts
- Resolving issues after version upgrades

**Safety:**
- Requires confirmation before executing
- Color-coded output for clarity
- Detailed summary of actions taken
- Never requires root privileges

---

### `generate-help.sh`

Generates HTML help documentation from Markdown sources using pandoc.

**Usage:**
```bash
./scripts/generate-help.sh
```

**What it does:**
- Converts `docs/user/*.md` to `resources/help/*.html`
- Applies custom template and CSS styling
- Automatically called during build process

**Requirements:**
- pandoc ≥ 3.0

---

## Development Notes

All scripts:
- Use `#!/bin/bash` shebang
- Include SPDX license headers
- Are marked executable (`chmod +x`)
- Follow shell best practices (`set -e`, quoted variables)
- Provide user-friendly output with color coding
