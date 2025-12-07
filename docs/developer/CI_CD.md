# CI/CD Pipeline Documentation

This document describes the GitHub Actions workflows configured for KeepTower.

## Workflows

### 1. CI Workflow (`.github/workflows/ci.yml`)

**Triggers:** Push to master/main/develop, Pull Requests

**Jobs:**

- **test** - Runs on Ubuntu 22.04 and latest
  - Installs all dependencies
  - Builds with Meson
  - Runs full test suite
  - Uploads test logs on failure

- **lint** - Code quality checks
  - Runs cppcheck for static analysis
  - Reports findings as artifacts

- **documentation** - Validates docs build
  - Generates Doxygen documentation
  - Uploads HTML docs as artifact (30 day retention)

- **security** - Security scanning
  - Runs Trivy vulnerability scanner
  - Uploads results to GitHub Security tab

### 2. Build Workflow (`.github/workflows/build.yml`)

**Triggers:** Push to master/main, Pull Requests

**Jobs:**

- **build-linux** - Multi-distro builds
  - Ubuntu 22.04, Ubuntu 24.04, Fedora 39
  - Creates tarballs for each platform
  - 14-day artifact retention

- **build-appimage** - Universal Linux binary
  - Creates portable AppImage
  - 30-day artifact retention

- **test-install** - Verifies build artifacts
  - Downloads and tests Ubuntu build
  - Validates binary dependencies

### 3. Release Workflow (`.github/workflows/release.yml`)

**Triggers:** Git tags matching `v*`, Manual dispatch

**Jobs:**

- **create-release** - Creates GitHub release
  - Extracts version from tag
  - Pulls changelog from CHANGELOG.md
  - Marks as prerelease if beta/alpha/rc

- **build-release-artifacts** - Builds release packages
  - Binary tarballs for Ubuntu 22/24, Fedora 39
  - AppImage universal binary
  - Source tarball from git
  - SHA256 checksums for all artifacts

- **build-documentation** - Release docs
  - Generates and packages API documentation
  - Uploads as release asset

## Using the CI/CD Pipeline

### Running Tests Locally

Before pushing, you can run tests locally:

```bash
meson setup build
meson compile -C build
meson test -C build
```

### Creating a Release

1. Update `CHANGELOG.md` with release notes
2. Update version in `meson.build` if needed
3. Commit changes
4. Create and push tag:
   ```bash
   git tag -a v0.2.0 -m "Release v0.2.0"
   git push origin v0.2.0
   ```
5. GitHub Actions will automatically:
   - Create the release
   - Build all platform binaries
   - Upload artifacts
   - Generate documentation

### Manual Release Trigger

You can also trigger a release manually:
1. Go to Actions → Release workflow
2. Click "Run workflow"
3. Enter version (e.g., `v0.1.1`)
4. Click "Run workflow"

## Artifacts

### CI Workflow Artifacts
- **test-logs-{os}**: Test failure logs (7 days)
- **cppcheck-report**: Static analysis results (7 days)
- **api-documentation**: HTML docs (30 days)

### Build Workflow Artifacts
- **keeptower-{distro}**: Platform binaries (14 days)
- **keeptower-AppImage**: Universal binary (30 days)

### Release Artifacts (Permanent)
- `keeptower-vX.Y.Z-ubuntu-22.04-x86_64.tar.gz`
- `keeptower-vX.Y.Z-ubuntu-24.04-x86_64.tar.gz`
- `keeptower-vX.Y.Z-fedora-39-x86_64.tar.gz`
- `keeptower-vX.Y.Z-x86_64.AppImage`
- `keeptower-vX.Y.Z-source.tar.gz`
- `keeptower-vX.Y.Z-docs.tar.gz`
- `checksums-*.txt` (SHA256 sums)

## Security Features

### Trivy Scanning
All code is scanned for:
- Known vulnerabilities in dependencies
- Exposed secrets
- Misconfigurations
- License issues

Results appear in the Security tab on GitHub.

### Code Quality
cppcheck performs static analysis checking for:
- Memory leaks
- Buffer overflows
- Null pointer dereferences
- Style issues
- Portability problems

## Badge Status

Add these badges to README.md:

```markdown
[![CI](https://github.com/USERNAME/keeptower/workflows/CI/badge.svg)](https://github.com/USERNAME/keeptower/actions/workflows/ci.yml)
[![Build](https://github.com/USERNAME/keeptower/workflows/Build/badge.svg)](https://github.com/USERNAME/keeptower/actions/workflows/build.yml)
```

Replace `USERNAME` with your GitHub username.

## Troubleshooting

### Workflow Fails on Dependency Installation
- Check if package names changed in newer Ubuntu/Fedora
- Verify all dependencies listed in README are included

### Tests Pass Locally but Fail in CI
- Check for environment-specific issues
- Review test logs artifact
- Ensure no hardcoded paths

### AppImage Build Fails
- linuxdeploy may have issues with GTK4
- Check that all required libraries are bundled
- Review linuxdeploy logs

### Release Not Created from Tag
- Ensure tag matches pattern `v*`
- Check CHANGELOG.md has section for this version
- Verify GitHub token has release permissions

## Future Enhancements

Potential additions:
- [ ] Code coverage reporting (codecov.io)
- [ ] Performance benchmarking
- [ ] Flatpak builds
- [ ] Snap package builds
- [ ] Windows builds (MinGW)
- [ ] macOS builds
- [ ] Docker container images
- [ ] Automated dependency updates (Dependabot)
