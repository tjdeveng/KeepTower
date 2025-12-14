#!/bin/bash
# Install git hooks for KeepTower development

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOOKS_DIR="${SCRIPT_DIR}/.git/hooks"

# Create hooks directory if it doesn't exist
mkdir -p "$HOOKS_DIR"

# Install pre-commit hook
cat > "${HOOKS_DIR}/pre-commit" << 'EOF'
#!/bin/bash
# Pre-commit hook to sync version from meson.build to README.md

# Extract version from meson.build
VERSION=$(grep "version:" meson.build | sed -E "s/.*version: '([^']+)'.*/\1/")

if [ -z "$VERSION" ]; then
    echo "Warning: Could not extract version from meson.build"
    exit 0
fi

# Update README.md version stamp at the end
# Use perl for more reliable regex replacement
perl -i -pe "s/^v\d+\.\d+\.\d+(-[a-z]+)? - /v${VERSION} - /" README.md

# Update version badge in README.md
perl -i -pe "s/version-\d+\.\d+\.\d+(-[a-z]+)?-orange/version-${VERSION}-orange/" README.md

# Add README.md if it was modified
git add README.md

echo "Updated README.md to version $VERSION"
EOF

# Make hook executable
chmod +x "${HOOKS_DIR}/pre-commit"

echo "Git hooks installed successfully!"
echo "The pre-commit hook will now automatically update README.md version from meson.build"
