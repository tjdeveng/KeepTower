# YubiKey Seam Refactor Plan

## Purpose

This plan defines the smallest useful seam for YubiKey-dependent code so KeepTower can:

- improve deterministic test coverage without requiring hardware,
- reduce hotspot size and responsibility in `VaultManager` and `MainWindow`,
- preserve GTK/Glib thread-safety for all YubiKey actions,
- preserve a real-hardware validation lane without making it the primary development gate.

This work should start under issue `#30` because it directly reduces hotspot responsibility in the current coupling points. If the effort expands into broader YubiKey library redesign, spin out a dedicated follow-up issue and cross-link it from `#30`.

## Current State

The codebase already has one useful boundary:

- `KeepTower::VaultYubiKeyService` centralizes several vault-facing hardware operations.

However, the main hotspot files still construct `YubiKeyManager` directly in workflow code:

- `src/core/VaultManager.cc`
- `src/core/VaultManagerV2.cc`
- `src/ui/windows/MainWindow.cc`
- several UI handlers and service implementations

That means test code can mock some orchestrated flows, but not the most coverage-relevant vault open/authentication paths.

## Problem Statement

The present design mixes three concerns in the same call paths:

1. vault/application policy decisions,
2. hardware orchestration and device-state handling,
3. direct `YubiKeyManager` API usage.

That coupling causes three practical problems:

1. hardware branches are difficult to cover in CI or local default test runs,
2. `VaultManager` and `MainWindow` stay responsibility-dense,
3. unit and integration tests cannot reliably force device success/failure cases.

## Design Goals

1. Move direct `YubiKeyManager` construction behind a narrow injectable boundary.
2. Keep the seam small enough to land incrementally.
3. Preserve current user-visible behavior and storage formats.
4. Support deterministic fake-driven tests for core vault flows.
5. Keep real-hardware characterization tests available as a secondary validation lane.
6. Preserve thread-safe behavior for all YubiKey-triggered UI flows.

## Non-Goals

1. Rewriting the entire YubiKey subsystem in one pass.
2. Changing vault on-disk schema as part of the seam work.
3. Making every UI path fully unit-testable in the first slice.
4. Requiring hardware validation before every local commit.
5. Making `VaultManager` itself globally thread-safe.

## Thread-Safety Requirement

This requirement is mandatory.

All YubiKey actions that can be triggered from GTK/Glib-driven flows must remain thread-safe at the integration boundary. Past segfaults have shown that unsafe cross-thread UI interaction and poorly isolated blocking hardware operations are not acceptable regressions.

The refactor must preserve the current project direction documented in `docs/developer/YUBIKEY_THREADING_COMPLETE.md`:

1. `VaultManager` itself remains intentionally non-thread-safe.
2. Blocking YubiKey work runs off the GTK main thread.
3. UI callbacks return to the main thread via GTK/Glib-safe mechanisms such as `Glib::Dispatcher` or equivalent existing patterns.
4. Direct libfido2/YubiKey access must be serialized where required by the underlying library/runtime assumptions.

This seam is therefore not only about testability. It is also a boundary for safe threading behavior.

## Recommended Seam

Use a two-layer seam rather than mocking `YubiKeyManager` directly everywhere.

### Layer 1: Vault-facing operations port

Define an interface for vault/application workflows, for example `IVaultYubiKeyService`, with operations that match current caller intent rather than raw libfido2 mechanics.

This interface should remain UI-thread agnostic: callers may invoke synchronous methods from worker-thread wrappers, but any UI updates must stay outside the service and return to the GTK main thread explicitly.

Candidate operations:

- detect devices
- get device info
- enroll yubikey
- perform challenge-response
- verify connected device identity
- create/set credential where needed

The existing `VaultYubiKeyService` becomes the production implementation of that interface.

### Layer 2: Low-level hardware adapter

Inside the service layer, hide direct `YubiKeyManager` construction behind a smaller adapter, for example `IYubiKeyHardware` or `IYubiKeyManagerAdapter`.

This layer should be responsible for enforcing any single-operation-at-a-time or serialization guarantees required by the hardware/library boundary.

Candidate operations:

- initialize
- enumerate devices
- get device info
- challenge_response
- create_credential
- set_credential
- is_yubikey_present

This layer is not the first testing target for application flows; it exists so the production service can be tested without depending on physical hardware for every case.

## Why This Shape

This matches patterns already present in the codebase:

- `VaultCreationOrchestrator` already accepts injected services for testability.
- `VaultYubiKeyService` already acts like an application service boundary, but it is concrete and internally constructs hardware dependencies.
- `YubiKeyThreadedOperations` already isolates blocking YubiKey work from the GTK main thread and uses GTK/Glib-safe callback dispatch.

So the smallest coherent move is:

1. formalize the service as an interface,
2. inject it into hotspot owners,
3. optionally add the lower-level adapter when the service internals need deterministic tests.

## First Refactor Slice

Start with the path that offers the best balance of coverage gain and contained blast radius.

### Slice 1 target

Refactor YubiKey-dependent credential verification/open-flow logic in:

- `src/core/VaultManager.cc`

Reason:

- it is a major coverage hotspot,
- it currently constructs `YubiKeyManager` directly in high-value auth paths,
- it is central to issue `#30` hotspot reduction.

### Slice 1 changes

1. Introduce `IVaultYubiKeyService`.
2. Make `VaultYubiKeyService` implement it.
3. Add injectable ownership to `VaultManager` with a production default.
4. Route one narrow hardware-dependent flow through the service instead of direct `YubiKeyManager` construction.
5. Add fake-service tests covering success, mismatch, missing device, timeout/error, and unsupported-hardware paths.
6. Preserve the existing worker-thread to main-thread handoff pattern for any UI-facing completion/progress callbacks.

## Follow-On Slices

### Slice 2

Apply the same seam to YubiKey-dependent paths in:

- `src/core/VaultManagerV2.cc`

### Slice 3

Move UI-triggered direct hardware interactions in:

- `src/ui/windows/MainWindow.cc`
- extracted UI handlers that still construct `YubiKeyManager` directly

The UI should depend on higher-level YubiKey workflow services or handler ports, not on the raw hardware manager.
The UI must not directly observe worker-thread callbacks from hardware/service code.

### Slice 4

If needed, introduce `IYubiKeyHardware` under `VaultYubiKeyService` and add focused tests for service-level hardware error mapping.

## Proposed Interfaces

Exact naming can follow repository conventions, but the shape should stay narrow.

### `IVaultYubiKeyService`

Responsibilities:

- expose vault-facing YubiKey operations,
- return existing `VaultResult<T>` / `VaultError` semantics,
- keep hardware details out of `VaultManager` and UI layers.

Suggested construction model:

- `VaultManager` accepts `std::shared_ptr<IVaultYubiKeyService>` with a default production instance,
- UI handlers accept the same interface or a thinner UI-specific port where appropriate.

Suggested threading contract:

- methods may block and should be invoked from worker-thread wrappers when used in interactive UI flows,
- methods must not touch GTK objects,
- results consumed by UI must be marshalled back to the main thread by the caller-side wrapper.

### `IYubiKeyHardware`

Responsibilities:

- isolate `YubiKeyManager` lifecycle and direct library calls,
- let `VaultYubiKeyService` be tested deterministically,
- keep hardware-specific error translation in one place.

Suggested construction model:

- `VaultYubiKeyService` owns a `std::unique_ptr<IYubiKeyHardware>` or receives a factory.

Suggested threading contract:

- one active hardware operation per adapter instance unless the underlying library is proven safe for more,
- internal synchronization is explicit rather than assumed,
- no UI-thread assumptions leak into this layer.

## Testing Strategy

### Primary gate

Default repository testing should stay hardware-free.

Add fake-driven tests for:

1. device absent,
2. initialization failure,
3. serial mismatch,
4. challenge-response failure,
5. successful response and key derivation flow,
6. validation/guard branches currently blocked by direct construction.

Also keep explicit regression coverage for threading behavior:

1. worker-thread completion is marshalled back to the main thread boundary,
2. concurrent start attempts are rejected or serialized deterministically,
3. cancellation/busy-state behavior does not permit unsafe re-entry.

### Secondary lane

Keep or expand hardware characterization coverage separately:

1. a dedicated YubiKey manual runner or characterization suite,
2. optional local execution before pushing hardware-sensitive changes,
3. archived logs and coverage artifacts outside commit messages.

Commit messages should summarize hardware validation when relevant, but the detailed artifact should live in generated reports or CI/manual-run attachments.

## Risks

1. Constructor churn in `VaultManager` and related call sites.
2. Partial seam extraction could temporarily duplicate logic if slices are too broad.
3. UI paths may still bypass the seam if only core files are updated.
4. Over-design is a risk if the first slice tries to solve all YubiKey use cases at once.
5. Thread-safety regressions could reintroduce UI crashes or segfaults if callback ownership becomes ambiguous.

## Guardrails

1. Keep the first slice focused on one auth/open path.
2. Preserve existing public behavior and error mapping.
3. Prefer default production wiring so most call sites do not need immediate edits.
4. Add tests as each path migrates; do not defer the test payoff.
5. Do not move GTK object access into service or hardware layers.
6. Do not rely on callers to "remember" serialization requirements; encode them in the wrapper/adapter boundary.

## Definition of Done for the Seam Initiative

The seam is successful when:

1. `VaultManager` no longer directly constructs `YubiKeyManager` in the first migrated flow.
2. That flow is covered by deterministic non-hardware tests.
3. The refactor reduces responsibility in issue `#30` hotspot files.
4. Real-hardware validation remains available as a secondary lane, not the default gate.
5. The migrated flow preserves GTK/Glib-safe threading behavior and does not regress prior segfault fixes.

## Immediate Next Step

Implement Slice 1 in `VaultManager` first, then rerun coverage to measure whether the seam unlocks enough new branches to justify continuing the pattern into `VaultManagerV2` and UI code.