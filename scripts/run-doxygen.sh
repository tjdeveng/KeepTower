#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
template="$repo_root/Doxyfile"
configured_doxyfile=${1:-"$repo_root/build/Doxyfile.ci"}
output_directory=${2:-"$repo_root/build/docs/api"}
input_directory="$repo_root/src"
strip_from_path="$repo_root/src"

if [[ ! -f "$template" ]]; then
  echo "Missing Doxyfile template: $template" >&2
  exit 1
fi

project_version=$(grep "version:" "$repo_root/meson.build" | head -1 | sed "s/.*version: '\(.*\)'.*/\1/")
if [[ -z "$project_version" ]]; then
  echo "Failed to determine project version from meson.build" >&2
  exit 1
fi

mkdir -p "$(dirname "$configured_doxyfile")" "$output_directory"

sed \
  -e "s|@PROJECT_VERSION@|$project_version|g" \
  -e "s|@DOXYGEN_OUTPUT_DIRECTORY@|$output_directory|g" \
  -e "s|@DOXYGEN_INPUT_DIRECTORY@|$input_directory|g" \
  -e "s|@DOXYGEN_STRIP_FROM_PATH@|$strip_from_path|g" \
  "$template" > "$configured_doxyfile"

doxygen "$configured_doxyfile"