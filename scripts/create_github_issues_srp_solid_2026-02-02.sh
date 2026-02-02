#!/usr/bin/env bash
set -euo pipefail

REPO="tjdeveng/KeepTower"
LABELS=("maintainability" "srp" "security" "audit")

ensure_labels() {
  # gh will fail issue creation if labels do not exist; create/update them first.
  gh label create "maintainability" --repo "$REPO" --color "0E8A16" --description "Maintainability / code health" --force >/dev/null
  gh label create "srp" --repo "$REPO" --color "1D76DB" --description "Single Responsibility Principle / SOLID boundary work" --force >/dev/null
  gh label create "security" --repo "$REPO" --color "D93F0B" --description "Security relevant (even if refactor)" --force >/dev/null
  gh label create "audit" --repo "$REPO" --color "5319E7" --description "Work tracked as part of compliance/audit activities" --force >/dev/null
}

require_auth() {
  if ! gh auth status --hostname github.com >/dev/null 2>&1; then
    echo "Not logged into GitHub. Run: gh auth login" >&2
    exit 1
  fi
}

create_issue() {
  local title="$1"
  local body="$2"

  gh issue create \
    --repo "$REPO" \
    --title "$title" \
    --body "$body" \
    $(printf -- '--label %q ' "${LABELS[@]}") \
    >/dev/null
}

main() {
  require_auth
  ensure_labels

  create_issue \
    "[REFACTOR] Extract FipsProviderManager from VaultManager" \
    $'Hotspot: `src/core/VaultManager.{h,cc}`\n\nProblem: OpenSSL provider/FIPS state transitions are mixed into vault orchestration.\n\nProposed minimal change:\n- Introduce `src/core/crypto/FipsProviderManager.{h,cc}` (or similar)\n- Keep public API stable; `VaultManager` delegates\n\nAcceptance criteria:\n- Existing behavior preserved\n- No new sensitive logging\n- `"FIPS Mode Tests"` pass\n\nTest plan:\n- `meson test -C build --print-errorlogs "FIPS Mode Tests"`'

  create_issue \
    "[REFACTOR] Extract UsernameHashMigrationStrategy from VaultManagerV2" \
    $'Hotspot: `src/core/VaultManagerV2.cc`\n\nProblem: Authentication and migration strategy are interleaved, increasing audit complexity.\n\nProposed minimal change:\n- Extract migration strategy into `src/core/services/UsernameHashMigrationStrategy.{h,cc}`\n- Prefer side-effect-free lookup that returns a result; caller applies mutations\n\nAcceptance criteria:\n- Migration suite behavior unchanged\n- Avoid plaintext usernames at info-level in migration path\n\nTest plan:\n- `meson test -C build --print-errorlogs --suite migration`'

  create_issue \
    "[REFACTOR] Reduce include coupling in VaultManager.h (forward decl / Impl)" \
    $'Hotspot: `src/core/VaultManager.h`\n\nProblem: Header includes many heavy dependencies, amplifying coupling and rebuild cost.\n\nProposed minimal change:\n- Replace includes with forward declarations where possible\n- Move implementation-only includes to `.cc` or an internal `Impl`\n\nAcceptance criteria:\n- No public API break\n- Normal + ASan builds succeed'

  create_issue \
    "[REFACTOR] Split PreferencesDialog into pages + PreferencesPresenter" \
    $'Hotspot: `src/ui/dialogs/PreferencesDialog.cc`\n\nProblem: UI layout + settings persistence + validation + vault policy toggles in one class.\n\nProposed minimal change:\n- Extract per-tab widgets and a small presenter for GSettings read/write + validation\n\nTest plan:\n- `meson test -C build --print-errorlogs "UI Security Tests"`'

  create_issue \
    "[REFACTOR] Extract ThemeController and VaultUiCoordinator from MainWindow" \
    $'Hotspot: `src/ui/windows/MainWindow.cc`\n\nProblem: Theme plumbing, dialog wiring, and vault orchestration are coupled.\n\nProposed minimal change:\n- Extract `ThemeController` (settings + GNOME monitoring)\n- Extract `VaultUiCoordinator` (open/close/lock/unlock wiring)\n\nTest plan:\n- `meson test -C build --print-errorlogs "UI Security Tests"`'

  create_issue \
    "[REFACTOR] Split YubiKeyManager into init/discovery/protocol components" \
    $'Hotspot: `src/core/managers/YubiKeyManager.cc`\n\nProblem: libfido2 init, device discovery caching, protocol ops, and crypto helpers are mixed.\n\nProposed minimal change:\n- Extract init, discovery, and protocol responsibilities into separate internal helpers\n\nAcceptance criteria:\n- No behavioral change\n- Relevant YubiKey tests still pass'

  create_issue \
    "[REFACTOR] Refactor V2AuthenticationHandler into a testable flow controller" \
    $'Hotspot: `src/ui/managers/V2AuthenticationHandler.cc`\n\nProblem: Dialog sequencing + device probing + core auth + clipboard policy are coupled.\n\nProposed minimal change:\n- Introduce a flow controller and a view interface to keep UI thin\n\nAcceptance criteria:\n- No behavior change\n- UI security tests pass'

  create_issue \
    "[REFACTOR] Extract ImportFlowController / ExportFlowController from VaultIOHandler" \
    $'Hotspot: `src/ui/managers/VaultIOHandler.cc`\n\nProblem: UX flow, re-auth gating, filesystem ops, and parsing are mixed.\n\nProposed minimal change:\n- Extract flow controllers; keep `ImportExport::*` focused on parsing\n\nAcceptance criteria:\n- Export warning + re-auth gate behavior unchanged'

  create_issue \
    "[REFACTOR] Split ImportExport by format; add CSV edge-case tests" \
    $'Hotspot: `src/utils/ImportExport.cc`\n\nProblem: Multi-format parsing in one file; CSV quoting/newline edge cases are easy to regress.\n\nProposed minimal change:\n- Split into per-format compilation units while keeping the public namespace API stable\n- Add tests for tricky CSV cases (quotes, newlines)'

  create_issue \
    "[REFACTOR] Split MultiUserTypes model vs serializer/deserializer" \
    $'Hotspot: `src/core/MultiUserTypes.cc`\n\nProblem: Model + serialization/parsing + migration helpers are combined.\n\nProposed minimal change:\n- Separate model types from parser/serializer with shared strict bounds helpers\n\nAcceptance criteria:\n- Existing migration tests pass\n- No new parsing regressions'

  echo "Created SRP/SOLID refactor issues in $REPO"
}

main "$@"
