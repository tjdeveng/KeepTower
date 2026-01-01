#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 tjdeveng
#
# Generate HTML help documentation from markdown sources

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DOCS_DIR="$PROJECT_ROOT/docs/user"
OUTPUT_DIR="$PROJECT_ROOT/resources/help"
TEMPLATE="$OUTPUT_DIR/template.html"
CSS_FILE="$OUTPUT_DIR/css/help-style.css"

# Get version from meson.build
VERSION=$(grep "version:" "$PROJECT_ROOT/meson.build" | head -1 | sed "s/.*'\(.*\)'.*/\1/")

echo "=== Generating KeepTower Help Documentation ==="
echo "Version: $VERSION"
echo "Source: $DOCS_DIR"
echo "Output: $OUTPUT_DIR"
echo

# Check pandoc is installed
if ! command -v pandoc &> /dev/null; then
    echo "Error: pandoc is not installed"
    echo "Install with: sudo dnf install pandoc  (Fedora/RHEL)"
    echo "           or: sudo apt install pandoc  (Ubuntu/Debian)"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Convert each markdown file to HTML
for md_file in "$DOCS_DIR"/*.md; do
    if [ -f "$md_file" ]; then
        filename=$(basename "$md_file" .md)
        html_file="$OUTPUT_DIR/${filename}.html"

        echo "Converting: $filename.md → $filename.html"

        # Extract title from first # heading
        title=$(grep "^# " "$md_file" | head -1 | sed 's/^# //')

        pandoc "$md_file" \
            --from markdown \
            --to html5 \
            --standalone \
            --template="$TEMPLATE" \
            --toc \
            --toc-depth=3 \
            --css="$CSS_FILE" \
            --embed-resources \
            --metadata pagetitle="$title" \
            --metadata title="$title" \
            --metadata version="$VERSION" \
            --output="$html_file"
    fi
done

echo
echo "✓ Help documentation generated successfully!"
echo "  Files: $OUTPUT_DIR/*.html"
echo "  CSS: $CSS_FILE"
echo
echo "To view locally:"
echo "  xdg-open $OUTPUT_DIR/00-home.html"
