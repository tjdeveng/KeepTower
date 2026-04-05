#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
version_file="$repo_root/.github/doxygen-version.txt"

if [[ ! -f "$version_file" ]]; then
  echo "Missing Doxygen version file: $version_file" >&2
  exit 1
fi

version=$(tr -d '[:space:]' < "$version_file")
if [[ -z "$version" ]]; then
  echo "Doxygen version file is empty: $version_file" >&2
  exit 1
fi

install_root=${1:-/tmp/doxygen-install}
archive_root=${2:-/tmp/doxygen-download}
archive_name="doxygen-${version}.linux.bin.tar.gz"
download_url="https://github.com/doxygen/doxygen/releases/download/Release_${version//./_}/${archive_name}"

if [[ -x "$install_root/bin/doxygen" ]]; then
  "$install_root/bin/doxygen" --version
  exit 0
fi

rm -rf "$archive_root"
mkdir -p "$install_root" "$archive_root"

echo "Installing Doxygen $version from $download_url"
curl -L --fail --retry 3 --retry-delay 2 -o "$archive_root/$archive_name" "$download_url"
tar -xzf "$archive_root/$archive_name" -C "$archive_root"

doxygen_bin=$(find "$archive_root" -type f -path '*/bin/doxygen' | head -n 1)
if [[ -z "$doxygen_bin" ]]; then
  echo "Failed to locate doxygen binary in extracted archive" >&2
  exit 1
fi

extracted_root=$(cd "$(dirname "$doxygen_bin")/.." && pwd)
rm -rf "$install_root"
mkdir -p "$install_root"
cp -R "$extracted_root"/. "$install_root"/

"$install_root/bin/doxygen" --version