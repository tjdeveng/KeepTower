# YubiKeyManager SRP Components (Internal)

**Date:** 2026-02-22

## Goal

Keep `YubiKeyManager` as the stable public façade while enforcing Single Responsibility Principle (SRP) internally by offloading distinct responsibilities to focused internal helpers.

**Non-goals**
- Do not change any external call sites.
- Do not change any public structs or method signatures.
- Do not change observable behavior (error strings, logging, caching semantics).

## High-level Structure

`YubiKeyManager` remains the entry point used by core/services/UI. Internally it composes small SRP helpers located under `src/core/managers/yubikey/`.

Responsibilities are split as:
- Global init + global serialization (libfido2 thread-safety)
- Device discovery (enumeration + caching)
- Protocol operations (open device, CBOR info, credential creation, hmac-secret assertion)
- Async execution (worker thread + cancellation + GTK main-thread callback dispatch)

## Internal Components

### 1) Global init & serialization

File: `src/core/managers/yubikey/Fido2Global.h`

Provides:
- A single global init flag to ensure `fido_init(0)` runs once.
- A single global mutex used to serialize all libfido2 operations.

Rationale:
- libfido2 operations are treated as not safely concurrent across threads/devices.
- Centralizing the mutex avoids accidental partial-locking and keeps the concurrency model consistent.

### 2) Device discovery & caching

File: `src/core/managers/yubikey/Fido2Discovery.h`

Provides:
- Cached device path discovery with a fixed cache duration.
- Filtering logic for candidate devices.

Invariants:
- Discovery can be disabled for tests via `DISABLE_YUBIKEY_DETECT`.
- Caching behavior and log messages should remain stable to avoid behavioral drift.

### 3) Protocol operations (FIDO2/CTAP2)

File: `src/core/managers/yubikey/Fido2Protocol.h`

Provides a small stateful object responsible for libfido2 protocol sequences:
- Device open/close
- CBOR info parsing into `YubiKeyManager::YubiKeyInfo`
- Credential creation (ES256 + hmac-secret extension)
- Assertion using hmac-secret

Notes:
- The protocol object is used only under the global libfido2 mutex.
- `YubiKeyManager` remains responsible for user-facing policy checks (initialized state, FIPS enforcement, parameter validation, PIN sourcing) and delegates only the protocol steps.

### 4) Async execution (threading + GTK dispatch)

File: `src/core/managers/yubikey/AsyncRunner.h`

Provides:
- A single-operation runner with:
  - `std::thread` for blocking work
  - cancellation checks before/after work
  - `Glib::Dispatcher` to invoke callbacks on the GTK main thread

Invariants:
- At most one async operation per `YubiKeyManager` instance.
- Callback invocation stays on the GTK main thread.
- Cancellation does not change results; it prevents delivery of callbacks when requested.

## YubiKeyManager Responsibilities (After SRP Split)

`YubiKeyManager` stays responsible for:
- Public API stability and error reporting (`m_last_error`)
- High-level policy checks (initialized state, FIPS enforcement, supported algorithms)
- PIN sourcing rules (parameter first, optional environment fallback where implemented)
- Locking boundaries (holding the global mutex around libfido2 operations)
- Delegation to SRP helpers

## Testing

Guardrails:
- Characterization tests validate key behavior that must not change during refactors.
- A focused subset of unit tests runs without requiring a physical device by disabling detection.

Recommended workflow:
- Run the characterization tests after each refactor phase.
- Run full `meson test -C build` once internal wiring is stable.
