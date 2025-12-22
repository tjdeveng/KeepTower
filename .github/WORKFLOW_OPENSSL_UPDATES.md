# GitHub Actions Workflow Updates for OpenSSL 3.5

## Changes needed in .github/workflows/build.yml and ci.yml

### Add OpenSSL 3.5 cache step (before "Build on Ubuntu"):

```yaml
    - name: Cache OpenSSL 3.5
      if: startsWith(matrix.distro, 'ubuntu')
      id: cache-openssl
      uses: actions/cache@v4
      with:
        path: /tmp/openssl-install
        key: openssl-3.5.0-ubuntu-${{ hashFiles('.github/openssl-version.txt') }}
        restore-keys: |
          openssl-3.5.0-ubuntu-
```

### Add OpenSSL 3.5 build step (in "Build on Ubuntu" section, after libcorrect):

```yaml
        # Check OpenSSL version and build 3.5 if needed
        OPENSSL_VERSION=$(pkg-config --modversion openssl || echo "0.0.0")
        echo "System OpenSSL version: $OPENSSL_VERSION"

        if [ "${{ steps.cache-openssl.outputs.cache-hit }}" != "true" ]; then
          # Build OpenSSL 3.5 from source
          echo "Building OpenSSL 3.5.0 with FIPS support..."
          bash scripts/build-openssl-3.5.sh /tmp/openssl-build /tmp/openssl-install || exit 1
        else
          echo "Using cached OpenSSL 3.5"
        fi

        # Use custom OpenSSL if system version < 3.5
        export PKG_CONFIG_PATH="/tmp/openssl-install/lib/pkgconfig:$PKG_CONFIG_PATH"
        export LD_LIBRARY_PATH="/tmp/openssl-install/lib64:/tmp/openssl-install/lib:$LD_LIBRARY_PATH"

        echo "OpenSSL configuration:"
        pkg-config --modversion openssl || echo "WARNING: pkg-config cannot find openssl"
        pkg-config --cflags openssl || echo "WARNING: No cflags"
        pkg-config --libs openssl || echo "WARNING: No libs"
```

### Create version tracking file:

Create `.github/openssl-version.txt`:
```
3.5.0
```

This ensures cache invalidation when we update OpenSSL versions.

### Update meson configuration step:

```yaml
        echo "Configuring build with meson..."
        PKG_CONFIG_PATH="/tmp/openssl-install/lib/pkgconfig:$PKG_CONFIG_PATH" \
        meson setup build --buildtype=release --prefix=/usr || exit 1
```

### Update test execution:

```yaml
        echo "Running tests..."
        LD_LIBRARY_PATH="/tmp/openssl-install/lib64:/tmp/openssl-install/lib:$LD_LIBRARY_PATH" \
        meson test -C build --verbose || exit 1
```

## Implementation Notes

1. **Cache Duration**: OpenSSL builds take 5-10 minutes, so caching is essential
2. **Cache Key**: Includes version file hash to invalidate when updating OpenSSL
3. **Fallback**: If cache fails, builds from source automatically
4. **Library Path**: Must set LD_LIBRARY_PATH for runtime
5. **PKG_CONFIG_PATH**: Must set for meson to find OpenSSL

## Testing

Test the workflow with:
1. Fresh build (no cache)
2. Cached build
3. Cache invalidation (change .github/openssl-version.txt)

## Fedora 41/42 Handling

Fedora doesn't need custom OpenSSL build, so skip these steps:

```yaml
    - name: Build on Fedora
      if: matrix.distro == 'fedora-41' || matrix.distro == 'fedora-42'
      run: |
        # Fedora 43+ will have OpenSSL 3.5 in repos
        # For Fedora 41/42, we'll build from source like Ubuntu
```

If Fedora 42 is in the matrix, apply same OpenSSL build logic.
