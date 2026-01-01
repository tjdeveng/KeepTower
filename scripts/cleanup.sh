#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# KeepTower Settings Cleanup Script
#
# This script removes KeepTower configuration and cache files.
# Use this for:
# - Fresh install after uninstalling
# - Fixing corrupted settings
# - Resolving schema version conflicts
#
# IMPORTANT: This script does NOT remove vault files or backups.
# Your vault data remains untouched.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   KeepTower Settings Cleanup Script                  ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if GSettings schema exists
if ! gsettings list-schemas | grep -q "com.tjdeveng.keeptower"; then
    echo -e "${YELLOW}⚠ GSettings schema 'com.tjdeveng.keeptower' not found${NC}"
    echo "This is normal if KeepTower is not currently installed."
    echo ""
fi

# List what will be removed
echo -e "${YELLOW}This script will remove:${NC}"
echo "  • GSettings configuration (com.tjdeveng.keeptower)"
echo "  • Compiled GSchema cache (if present in ~/.local/share/glib-2.0)"
echo "  • Application config directory (~/.config/keeptower/)"
echo ""

echo -e "${GREEN}This script will NOT remove:${NC}"
echo "  ✓ Vault files (*.vault)"
echo "  ✓ Vault backups (*.vault.backup)"
echo "  ✓ Any user data"
echo ""

# Confirmation prompt
read -p "$(echo -e ${YELLOW}Continue with cleanup? [y/N]:${NC} )" -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${RED}Cleanup cancelled.${NC}"
    exit 0
fi

echo ""
echo -e "${BLUE}Starting cleanup...${NC}"
echo ""

# 1. Reset GSettings
if gsettings list-schemas | grep -q "com.tjdeveng.keeptower"; then
    echo -e "${YELLOW}→${NC} Resetting GSettings for com.tjdeveng.keeptower..."
    if gsettings reset-recursively com.tjdeveng.keeptower 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} GSettings reset successfully"
    else
        echo -e "  ${YELLOW}⚠${NC} GSettings reset skipped (not installed or already reset)"
    fi
else
    echo -e "${YELLOW}→${NC} GSettings schema not found, skipping..."
fi

# 2. Remove local GSchema cache (only if it exists)
GSCHEMA_CACHE="$HOME/.local/share/glib-2.0/schemas/gschemas.compiled"
if [[ -f "$GSCHEMA_CACHE" ]]; then
    echo -e "${YELLOW}→${NC} Removing local GSchema cache..."
    rm -f "$GSCHEMA_CACHE"
    echo -e "  ${GREEN}✓${NC} Removed $GSCHEMA_CACHE"
else
    echo -e "${YELLOW}→${NC} No local GSchema cache found"
fi

# 3. Remove application config directory (if exists)
CONFIG_DIR="$HOME/.config/keeptower"
if [[ -d "$CONFIG_DIR" ]]; then
    echo -e "${YELLOW}→${NC} Removing application config directory..."
    rm -rf "$CONFIG_DIR"
    echo -e "  ${GREEN}✓${NC} Removed $CONFIG_DIR"
else
    echo -e "${YELLOW}→${NC} No config directory found"
fi

# 4. Check for dconf database entries (informational only)
DCONF_PATH="/com/tjdeveng/keeptower/"
if command -v dconf >/dev/null 2>&1; then
    if dconf list "$DCONF_PATH" 2>/dev/null | grep -q .; then
        echo -e "${YELLOW}→${NC} Removing dconf entries..."
        dconf reset -f "$DCONF_PATH"
        echo -e "  ${GREEN}✓${NC} dconf entries cleared"
    else
        echo -e "${YELLOW}→${NC} No dconf entries found"
    fi
else
    echo -e "${YELLOW}→${NC} dconf not available, skipping..."
fi

echo ""
echo -e "${GREEN}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Cleanup Complete!                                   ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Your vault files and backups remain untouched."
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "  • Reinstall KeepTower if needed"
echo "  • Settings will be reset to defaults on next launch"
echo "  • Your vaults will open normally (no data loss)"
echo ""
