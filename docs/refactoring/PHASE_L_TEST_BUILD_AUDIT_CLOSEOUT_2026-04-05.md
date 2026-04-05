# Phase L Closeout: Test Build Boundary Cleanup and Audit Hardening

Date: 2026-04-05
Status: Closed

## Objective
Close the final architecture-audit slice by making the test build graph accurately reflect extracted library boundaries, clarifying the remaining intentional app-layer white-box coverage, and raising API documentation policy to a strict zero-warning standard.

This phase follows Phase K and serves as the final closeout slice for the original internal code-audit effort around architecture clarity, maintainability, and audit readiness.

## Baseline
- `tests/meson.build` still carried historical direct `../src/*.cc` inclusions after several library extractions.
- Some direct inclusions were still legitimate app-layer white-box coverage, but that intent was not consistently documented.
- A few remaining extracted-library test targets still bypassed their library dependencies.
- Doxygen could report a clean run while missing-doc warnings were disabled, which was not sufficient for audit-grade documentation policy.

## Scope
### In Scope
- Remove remaining extracted-library direct source inclusions from tests where proper Meson deps exist.
- Document which remaining direct source inclusions are intentional white-box or app-layer integration coverage.
- Tighten ambiguous app-layer test wiring so target names and dependencies match what the tests actually prove.
- Enforce strict zero-warning Doxygen coverage for the checked-in public API surface.

### Out of Scope
- New product features.
- Large-scale app-layer library extraction beyond what the current Meson target graph already supports.
- Platform certification or distro-specific runtime verification.

## Completed Slice Summary

### L.1 White-Box Policy Documentation
- Documented the remaining intentional direct source inclusions in `tests/meson.build` so extracted-library exceptions are reviewable instead of implicit.
- Commit: `5e81684 build(tests): document intentional white-box source inclusions`

### L.2 Final Extracted-Library Dependency Cleanup
- Removed the last extracted-library source bypasses from the test graph:
  - `../src/lib/fec/ReedSolomon.cc`
  - `../src/lib/crypto/KekDerivationService.cc`
- Updated those tests to link through `fec_dep` and `crypto_dep` instead.
- Commit: `354d982 build(tests): link fec and kek derivation tests through deps`

### L.3 App-Layer Audit Clarity Pass
- Split pure boundary-model coverage into `tests/test_vault_boundary_models.cc`.
- Narrowed `tests/test_vault_boundary_types.cc` to the `VaultManager` accessor/integration behavior it actually validates.
- Removed redundant `VaultManagerV2.cc` inclusions from app-layer test bundles already backed by shared source aggregation.
- Commit: `0e6c339 test(build): tighten app-layer audit clarity`

### L.4 Strict Doxygen Enforcement
- Audited public headers under strict undocumented/incomplete/param-doc warning settings.
- Filled the missing API documentation backlog across core, lib, and UI public headers.
- Reduced Doxygen input scope to the supported public API surface (`*.h`, `*.hpp`, `*.md`) and promoted warnings to errors in the checked-in configuration.
- Commit: `c70838e docs(api): enforce zero-warning doxygen coverage`

## Validation Performed
- Build/test validation during Phase L:
  - `meson compile -C build`
  - targeted `meson test -C build ... --print-errorlogs` for the affected dependency, repository, service, and UI-adjacent test targets
  - full `meson test -C build`
- Result progression:
  - after extracted-library cleanup: `63/63` passing
  - after app-layer audit clarity pass: `64/64` passing
- Documentation validation:
  - Meson-configured `doxygen` target passes cleanly
  - strict temporary Doxygen audit passed cleanly before policy promotion
  - checked-in strict Doxygen configuration now also passes cleanly

## Outcome
- The test build graph now aligns with the extracted library architecture for storage, vault format, crypto, backup, and FEC code.
- Remaining direct test source inclusions are documented as intentional white-box or app-layer integration coverage instead of accidental layering leakage.
- Boundary-model coverage is now separated from manager/app-layer accessor coverage, which makes audit review faster and more defensible.
- Public API documentation is enforced at zero warnings under the checked-in Doxygen policy.
- With Phases I, K, and L closed, the original internal architecture/code-audit program is complete from a repository structure, test-boundary, and API-documentation perspective.

## Exit Criteria
- No remaining extracted-library `.cc` inclusions in `tests/meson.build` where a dedicated Meson dependency exists.
- Remaining direct `.cc` inclusions documented and intentionally scoped.
- Full suite green after the build-graph cleanup.
- Checked-in Doxygen policy fails on missing public API documentation and passes cleanly in the validated tree.
- Roadmap and changelog updated to reflect phase completion.

## Risk Notes
- App-layer tests still rely on direct source inclusion where no dedicated library boundary exists yet; this is intentional, but future extractions should continue shrinking that surface.
- Strict Doxygen policy will now reject undocumented public API additions, so follow-on refactors need to keep header docs current as part of normal change flow.

## Phase Retrospective
- The highest-value Phase L work was not adding new code but making architectural intent explicit in the build graph and documentation policy.
- The main audit lesson is that green builds are not enough when the dependency graph or doc policy can still hide architectural drift.
- Closing the phase at the Meson-boundary and Doxygen-policy level materially improves future reviews, onboarding, and maintenance work.