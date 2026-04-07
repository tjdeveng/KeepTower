#!/usr/bin/env bash

set -euo pipefail

build_dir="${1:-build}"
report_dir="${2:-static-analysis-reports}"

mkdir -p "$report_dir"

cppcheck_report="$report_dir/cppcheck-report.txt"
clang_gate_report="$report_dir/clang-tidy-gated.txt"
clang_advisory_report="$report_dir/clang-tidy-advisory.txt"

if [[ -n "${CLANG_TIDY_BIN:-}" ]]; then
  clang_tidy_bin="$CLANG_TIDY_BIN"
elif command -v clang-tidy-20 >/dev/null 2>&1; then
  clang_tidy_bin="$(command -v clang-tidy-20)"
elif command -v clang-tidy >/dev/null 2>&1; then
  clang_tidy_bin="$(command -v clang-tidy)"
else
  echo "clang-tidy executable not found" >&2
  exit 1
fi

critical_files=(
  "src/core/VaultManager.cc"
  "src/core/VaultManagerV2.cc"
  "src/core/MultiUserTypes.cc"
  "src/lib/storage/VaultIO.cc"
  "src/lib/crypto/VaultCrypto.cc"
  "src/lib/crypto/VaultCryptoService.cc"
  "src/core/services/VaultFileService.cc"
  "src/ui/managers/VaultIOHandler.cc"
)

# Tightened enforcement for issue #27:
# - Keep the gate scoped to the audited critical-file subset.
# - Hard-gate the zero-baseline clang-analyzer categories plus the two
#   bugprone checks that were cleared in issue #26.
gate_checks='-*,clang-analyzer-core.*,clang-analyzer-cplusplus.*,clang-analyzer-security.*,bugprone-exception-escape,bugprone-unchecked-optional-access'
advisory_checks=''

sanitize_clang_tidy_output() {
  sed -E \
    -e '/^[0-9]+ warnings generated\.$/d' \
    -e '/^Suppressed [0-9]+ warnings.*/d' \
    -e '/^Use -header-filter=.*/d' \
    -e '/^$/d'
}

run_clang_tidy_group() {
  local checks="$1"
  local outfile="$2"
  local fail_on_findings="$3"
  local status=0

  : > "$outfile"

  for file in "${critical_files[@]}"; do
    local raw_output
    local sanitized_output
    local invocation_status=0

    raw_output=$(mktemp)
    sanitized_output=$(mktemp)

    if "$clang_tidy_bin" -p "$build_dir" \
      --quiet \
      --system-headers=false \
      --checks="$checks" \
      "$file" > "$raw_output" 2>&1; then
      invocation_status=0
    else
      invocation_status=$?
    fi

    sanitize_clang_tidy_output < "$raw_output" > "$sanitized_output"

    if [[ -s "$sanitized_output" ]]; then
      {
        echo "### $file"
        cat "$sanitized_output"
        echo
      } >> "$outfile"

      if [[ "$fail_on_findings" == "true" ]]; then
        status=1
      fi
    elif [[ $invocation_status -ne 0 ]]; then
      {
        echo "### $file"
        cat "$raw_output"
        echo
      } >> "$outfile"
      status=1
    fi

    rm -f "$raw_output" "$sanitized_output"
  done

  return "$status"
}

echo "Running cppcheck report..."
cppcheck --enable=warning,style,performance,portability \
  --error-exitcode=0 \
  --inline-suppr \
  --suppress=missingIncludeSystem \
  --std=c++23 \
  src/ > "$cppcheck_report" 2>&1 || true

echo "Running clang-tidy gated checks on audited critical files..."
gate_status=0
if ! run_clang_tidy_group "$gate_checks" "$clang_gate_report" true; then
  gate_status=1
fi

if [[ -n "$advisory_checks" ]]; then
  echo "Running clang-tidy advisory checks on audited critical files..."
  run_clang_tidy_group "$advisory_checks" "$clang_advisory_report" false || true
else
  : > "$clang_advisory_report"
fi

if [[ $gate_status -ne 0 ]]; then
  echo "Static-analysis gate failed. See $clang_gate_report for details." >&2
  exit 1
fi

echo "Static-analysis gate passed. Advisory findings, if any, are in $clang_advisory_report."