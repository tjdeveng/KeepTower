#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 tjdeveng
#
# package-windows.sh — Bundle KeepTower and all runtime dependencies into a
# self-contained Windows ZIP archive.
#
# Must be run inside an MSYS2 MinGW64 shell where the executable has already
# been built by Meson.
#
# Usage:
#   bash scripts/package-windows.sh <version> <build_dir> <output_dir>
#
# Example:
#   bash scripts/package-windows.sh v0.4.0 build dist
#
# Produces:  <output_dir>/keeptower-<version>-windows-x86_64.zip

set -euo pipefail

VERSION="${1:-dev}"
BUILD_DIR="${2:-build}"
OUTPUT_DIR="${3:-dist}"
MINGW_PREFIX="${MINGW_PREFIX:-/mingw64}"
MINGW_BIN="${MINGW_PREFIX}/bin"
DIST_NAME="keeptower-${VERSION}-windows-x86_64"
DIST_DIR="${OUTPUT_DIR}/${DIST_NAME}"

echo "=== KeepTower Windows Packager ==="
echo "Version    : ${VERSION}"
echo "Build dir  : ${BUILD_DIR}"
echo "Output dir : ${OUTPUT_DIR}"
echo "MinGW prefix: ${MINGW_PREFIX}"

# ----------------------------------------------------------------------------
# 1. Locate the executable
# ----------------------------------------------------------------------------
EXE=""
for candidate in \
    "${BUILD_DIR}/src/keeptower.exe" \
    "${BUILD_DIR}/src/keeptower" \
    "${BUILD_DIR}/keeptower.exe" \
    "${BUILD_DIR}/keeptower"; do
    if [ -f "${candidate}" ]; then
        EXE="${candidate}"
        break
    fi
done

if [ -z "${EXE}" ]; then
    echo "ERROR: keeptower executable not found in ${BUILD_DIR}"
    exit 1
fi
echo "Executable : ${EXE}"

# ----------------------------------------------------------------------------
# 2. Create staging directory
# ----------------------------------------------------------------------------
rm -rf "${DIST_DIR}"
mkdir -p "${DIST_DIR}"

# Ensure the exe has a .exe suffix in the output (MSYS2 may omit it)
cp "${EXE}" "${DIST_DIR}/keeptower.exe"

# ----------------------------------------------------------------------------
# 3. Bundle all required DLLs via ldd (catches transitive deps automatically)
# ----------------------------------------------------------------------------
echo "Collecting DLLs via ldd..."
ldd "${DIST_DIR}/keeptower.exe" \
    | grep -i "${MINGW_PREFIX}" \
    | awk '{print $3}' \
    | sort -u \
    | while read -r dll; do
        if [ -f "${dll}" ]; then
            cp "${dll}" "${DIST_DIR}/"
            echo "  + $(basename ${dll})"
        fi
    done

# ldd only reports direct dependencies; some GTK4 plugins and protobuf's abseil
# dependency load DLLs at runtime. Copy them explicitly.

# All abseil DLLs (protobuf links against many at runtime)
echo "Collecting abseil DLLs..."
for dll in "${MINGW_BIN}"/libabsl_*.dll; do
    [ -f "${dll}" ] || continue
    if [ ! -f "${DIST_DIR}/$(basename ${dll})" ]; then
        cp "${dll}" "${DIST_DIR}/"
        echo "  + $(basename ${dll}) (abseil)"
    fi
done

# All GStreamer DLLs (GTK4 on MSYS2 is built with GStreamer media backend;
# there are many libgst* DLLs loaded at runtime by the media subsystem)
echo "Collecting GStreamer DLLs..."
for dll in "${MINGW_BIN}"/libgst*.dll; do
    [ -f "${dll}" ] || continue
    if [ ! -f "${DIST_DIR}/$(basename ${dll})" ]; then
        cp "${dll}" "${DIST_DIR}/"
        echo "  + $(basename ${dll}) (gstreamer)"
    fi
done

# GStreamer codec plugins (loaded at runtime from lib/gstreamer-1.0/)
GST_PLUGINS_SRC="${MINGW_PREFIX}/lib/gstreamer-1.0"
if [ -d "${GST_PLUGINS_SRC}" ]; then
    mkdir -p "${DIST_DIR}/lib/gstreamer-1.0"
    cp "${GST_PLUGINS_SRC}/"*.dll "${DIST_DIR}/lib/gstreamer-1.0/" 2>/dev/null || true
    echo "  + gstreamer-1.0 plugins"
fi

# liborc — GStreamer SIMD optimisation library (NOT prefixed libgst*, missed by glob)
for dll in "${MINGW_BIN}"/liborc*.dll; do
    [ -f "${dll}" ] || continue
    if [ ! -f "${DIST_DIR}/$(basename ${dll})" ]; then
        cp "${dll}" "${DIST_DIR}/"
        echo "  + $(basename ${dll}) (orc/gstreamer)"
    fi
done

# utf8_validity / utf8_range — split out from abseil in newer protobuf builds
for dll in \
    libutf8_validity.dll \
    libutf8_range.dll; do
    if [ -f "${MINGW_BIN}/${dll}" ] && [ ! -f "${DIST_DIR}/${dll}" ]; then
        cp "${MINGW_BIN}/${dll}" "${DIST_DIR}/"
        echo "  + ${dll} (utf8/protobuf)"
    fi
done

# cairo-script-interpreter (shipped with the cairo package on MSYS2)
for dll in libcairo-script-interpreter-2.dll; do
    if [ -f "${MINGW_BIN}/${dll}" ] && [ ! -f "${DIST_DIR}/${dll}" ]; then
        cp "${MINGW_BIN}/${dll}" "${DIST_DIR}/"
        echo "  + ${dll} (cairo)"
    fi
done

# libcorrect — built from source and installed to /mingw64; ldd may miss it
# if the build system linked it statically or it lives in a non-standard path.
for search_path in \
    "${MINGW_BIN}/libcorrect.dll" \
    "${BUILD_DIR}/libcorrect.dll" \
    "${BUILD_DIR}/src/libcorrect.dll" \
    "/mingw64/bin/libcorrect.dll" \
    "/mingw64/lib/libcorrect.dll"; do
    if [ -f "${search_path}" ] && [ ! -f "${DIST_DIR}/libcorrect.dll" ]; then
        cp "${search_path}" "${DIST_DIR}/libcorrect.dll"
        echo "  + libcorrect.dll (from ${search_path})"
        break
    fi
done
[ -f "${DIST_DIR}/libcorrect.dll" ] || echo "  (libcorrect.dll not found — may be statically linked)"

# Explicitly add runtime DLLs that ldd misses (dlopen'd, indirect deps, etc.)
# Grouped by dependency chain for maintainability.
RUNTIME_DLLS=(
    # GTK4 / GDK core
    libgtk-4-1.dll
    libgdk_pixbuf-2.0-0.dll
    libgraphene-1.0-0.dll
    libepoxy-0.dll
    libadwaita-1-0.dll
    # Pango + text rendering
    libpango-1.0-0.dll
    libpangocairo-1.0-0.dll
    libpangowin32-1.0-0.dll
    libpangoft2-1.0-0.dll
    libfribidi-0.dll
    libharfbuzz-0.dll
    libharfbuzz-gobject-0.dll
    libharfbuzz-subset-0.dll
    libgraphite2.dll
    libthai-0.dll
    libdatrie-1.dll
    # Cairo
    libcairo-2.dll
    libcairo-gobject-2.dll
    libcairomm-1.16-1.dll
    libpixman-1-0.dll
    # Font rendering
    libfontconfig-1.dll
    libfreetype-6.dll
    # Image codecs (pixbuf loaders)
    libpng16-16.dll
    libjpeg-8.dll
    libtiff-6.dll
    libwebp-7.dll
    libwebpdecoder-3.dll
    libwebpdemux-2.dll
    libsharpyuv-0.dll
    libjbig-0.dll
    libdeflate.dll
    liblerc.dll
    libopenjp2-7.dll
    # Compression / encoding
    zlib1.dll
    libbz2-1.dll
    liblzma-5.dll
    libbrotlidec.dll
    libbrotlicommon.dll
    libzstd.dll
    liblzo2-2.dll
    # GLib / GIO / GObject
    libglib-2.0-0.dll
    libgobject-2.0-0.dll
    libgio-2.0-0.dll
    libgmodule-2.0-0.dll
    libgthread-2.0-0.dll
    libpcre2-8-0.dll
    libffi-8.dll
    libintl-8.dll
    libiconv-2.dll
    libexpat-1.dll
    # libfido2 deps
    libcbor-0.11.dll
    libcbor.dll
    libcrypto-3-x64.dll
    libssl-3-x64.dll
    # C++ / runtime
    libstdc++-6.dll
    libgcc_s_seh-1.dll
    libwinpthread-1.dll
)
for dll in "${RUNTIME_DLLS[@]}"; do
    if [ -f "${MINGW_BIN}/${dll}" ] && [ ! -f "${DIST_DIR}/${dll}" ]; then
        cp "${MINGW_BIN}/${dll}" "${DIST_DIR}/"
        echo "  + ${dll} (runtime)"
    fi
done

# ----------------------------------------------------------------------------
# 4. GDK-Pixbuf loaders (needed to render icons/images)
# ----------------------------------------------------------------------------
echo "Copying GDK-Pixbuf loaders..."
PIXBUF_LOADERS_DIR="${MINGW_PREFIX}/lib/gdk-pixbuf-2.0"
if [ -d "${PIXBUF_LOADERS_DIR}" ]; then
    mkdir -p "${DIST_DIR}/lib"
    cp -r "${PIXBUF_LOADERS_DIR}" "${DIST_DIR}/lib/"

    # Update the loaders.cache path to be relative to the bundle
    LOADERS_CACHE="${DIST_DIR}/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"
    if [ -f "${LOADERS_CACHE}" ]; then
        # Rewrite absolute paths to relative ones inside the bundle
        sed -i "s|${MINGW_PREFIX}/lib/gdk-pixbuf-2.0/2.10.0/loaders/||g" "${LOADERS_CACHE}"
    fi

    # Copy the loader DLLs themselves
    for loader_dll in "${MINGW_PREFIX}/lib/gdk-pixbuf-2.0/2.10.0/loaders/"*.dll; do
        [ -f "${loader_dll}" ] && cp "${loader_dll}" "${DIST_DIR}/lib/gdk-pixbuf-2.0/2.10.0/loaders/" || true
    done
else
    echo "  (gdk-pixbuf loaders directory not found — skipping)"
fi

# ----------------------------------------------------------------------------
# 5. GLib/GSettings schemas
# ----------------------------------------------------------------------------
echo "Compiling GSettings schemas..."
mkdir -p "${DIST_DIR}/share/glib-2.0/schemas"

# App's own schema
SCHEMA_FILE=$(find "${BUILD_DIR}" -name "*.gschema.xml" | head -1)
if [ -n "${SCHEMA_FILE}" ]; then
    cp "${SCHEMA_FILE}" "${DIST_DIR}/share/glib-2.0/schemas/"
fi
# Also copy from source data/
for schema in data/*.gschema.xml; do
    [ -f "${schema}" ] && cp "${schema}" "${DIST_DIR}/share/glib-2.0/schemas/" || true
done

# System schemas needed by GTK4/GLib settings
for schema_dir in \
    "${MINGW_PREFIX}/share/glib-2.0/schemas" \
    "${MINGW_PREFIX}/share/gsettings-desktop-schemas"; do
    if [ -d "${schema_dir}" ]; then
        cp "${schema_dir}/"*.gschema.xml "${DIST_DIR}/share/glib-2.0/schemas/" 2>/dev/null || true
    fi
done

glib-compile-schemas "${DIST_DIR}/share/glib-2.0/schemas/"
echo "  + gschemas.compiled"

# ----------------------------------------------------------------------------
# 6. Icon theme (Adwaita + hicolor fallback)
# ----------------------------------------------------------------------------
echo "Copying icon themes..."
for theme in Adwaita hicolor; do
    SRC="${MINGW_PREFIX}/share/icons/${theme}"
    if [ -d "${SRC}" ]; then
        mkdir -p "${DIST_DIR}/share/icons"
        cp -r "${SRC}" "${DIST_DIR}/share/icons/"
        echo "  + icons/${theme}"
    fi
done

# App icon
APP_ICON=$(find data/ resources/ -name "*.svg" -path "*/icons/*" 2>/dev/null | head -1)
if [ -n "${APP_ICON}" ]; then
    mkdir -p "${DIST_DIR}/share/icons/hicolor/scalable/apps"
    cp "${APP_ICON}" "${DIST_DIR}/share/icons/hicolor/scalable/apps/"
fi

# ----------------------------------------------------------------------------
# 7. App data files (help, UI resources)
# ----------------------------------------------------------------------------
echo "Copying app data files..."
if [ -d "${BUILD_DIR}/data" ]; then
    mkdir -p "${DIST_DIR}/share/keeptower"
    cp -r "${BUILD_DIR}/data/"* "${DIST_DIR}/share/keeptower/" 2>/dev/null || true
fi
for dir in resources/help resources/ui; do
    if [ -d "${dir}" ]; then
        mkdir -p "${DIST_DIR}/share/keeptower/$(basename ${dir})"
        cp -r "${dir}/"* "${DIST_DIR}/share/keeptower/$(basename ${dir})/" 2>/dev/null || true
    fi
done

# ----------------------------------------------------------------------------
# 7a. Fontconfig runtime config
# Needed so fontconfig can locate fonts on the target Windows machine.
# Without this, text rendering silently fails or falls back to no-font.
# ----------------------------------------------------------------------------
FONTCONFIG_SRC="${MINGW_PREFIX}/etc/fonts"
if [ -d "${FONTCONFIG_SRC}" ]; then
    mkdir -p "${DIST_DIR}/etc/fonts"
    cp -r "${FONTCONFIG_SRC}/"* "${DIST_DIR}/etc/fonts/"
    echo "  + fontconfig config"
fi

# ----------------------------------------------------------------------------
# 8. GLib type modules / gio modules
# ----------------------------------------------------------------------------
GIO_MODULES="${MINGW_PREFIX}/lib/gio/modules"
if [ -d "${GIO_MODULES}" ]; then
    mkdir -p "${DIST_DIR}/lib/gio/modules"
    cp "${GIO_MODULES}/"*.dll "${DIST_DIR}/lib/gio/modules/" 2>/dev/null || true
    cp "${GIO_MODULES}/giomodule.cache" "${DIST_DIR}/lib/gio/modules/" 2>/dev/null || true
    echo "  + gio modules"
fi

# ----------------------------------------------------------------------------
# 9. Launcher batch file (sets environment for portable run)
# ----------------------------------------------------------------------------
cat > "${DIST_DIR}/keeptower-launch.bat" << 'BATCH'
@echo off
:: KeepTower launcher — sets GTK environment for portable bundle
setlocal
set "BUNDLE=%~dp0"
set "GSETTINGS_SCHEMA_DIR=%BUNDLE%share\glib-2.0\schemas"
set "GDK_PIXBUF_MODULE_FILE=%BUNDLE%lib\gdk-pixbuf-2.0\2.10.0\loaders.cache"
set "XDG_DATA_DIRS=%BUNDLE%share"
set "GIO_MODULE_DIR=%BUNDLE%lib\gio\modules"
set "FONTCONFIG_PATH=%BUNDLE%etc\fonts"
set "GST_PLUGIN_PATH=%BUNDLE%lib\gstreamer-1.0"
start "" "%BUNDLE%keeptower.exe"
BATCH
echo "  + keeptower-launch.bat"

# ----------------------------------------------------------------------------
# 10. Docs
# ----------------------------------------------------------------------------
for f in README.md LICENSE CHANGELOG.md; do
    [ -f "${f}" ] && cp "${f}" "${DIST_DIR}/" || true
done

# ----------------------------------------------------------------------------
# 11. NSIS installer (if makensis is available)
# ----------------------------------------------------------------------------
INSTALLER_FILE="${OUTPUT_DIR}/keeptower-${VERSION}-setup.exe"
if command -v makensis &>/dev/null; then
    echo "Building NSIS installer..."
    # Convert MSYS2 paths to Windows paths for makensis
    WIN_DIST_DIR=$(cygpath -w "${DIST_DIR}")
    WIN_OUTFILE=$(cygpath -w "${INSTALLER_FILE}")
    makensis \
        -DVERSION="${VERSION}" \
        -DDIST_DIR="${WIN_DIST_DIR}" \
        -DOUTFILE="${WIN_OUTFILE}" \
        "scripts/keeptower.nsi"
    if [ -f "${INSTALLER_FILE}" ]; then
        SHA_INST=$(sha256sum "${INSTALLER_FILE}" | awk '{print $1}')
        echo "${SHA_INST}  keeptower-${VERSION}-setup.exe" \
            >> "${OUTPUT_DIR}/checksums-windows.txt"
        echo "  + installer: ${INSTALLER_FILE}"
    fi
else
    echo "  (makensis not found — skipping installer, ZIP only)"
fi

# ----------------------------------------------------------------------------
# 12. Create ZIP
# ----------------------------------------------------------------------------
ZIP_FILE="${OUTPUT_DIR}/keeptower-${VERSION}-windows-x86_64.zip"
echo "Creating ZIP archive: ${ZIP_FILE}"
cd "${OUTPUT_DIR}"
zip -r "keeptower-${VERSION}-windows-x86_64.zip" "${DIST_NAME}/"
cd - > /dev/null

SHA=$(sha256sum "${ZIP_FILE}" | awk '{print $1}')
echo "${SHA}  keeptower-${VERSION}-windows-x86_64.zip" \
    >> "${OUTPUT_DIR}/checksums-windows.txt"

echo ""
echo "=== Package complete ==="
echo "Archive : ${ZIP_FILE}"
[ -f "${INSTALLER_FILE}" ] && echo "Installer: ${INSTALLER_FILE}"
echo "Contents:"
ls -lh "${DIST_DIR}/"
