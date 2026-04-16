# Manual Tests

This directory contains manual and developer-only test programs and scripts that are not part of the automated Meson test suite. They exist for focused debugging, smoke-testing subsystems, or verifying behaviour that is difficult to automate.

Run all scripts from the **project root** unless noted otherwise. Build the project first: `ninja -C build`.

---

## Documented test guides

These have their own step-by-step guides:

| File | Purpose |
|---|---|
| [`MANUAL_TEST_UNDO_REDO.md`](MANUAL_TEST_UNDO_REDO.md) | Step-by-step guide for verifying undo/redo bug fixes |
| [`QUICK_INSTALL_TEST.md`](QUICK_INSTALL_TEST.md) | Verifies system installation via `meson install` |

---

## Standalone programs (require manual compilation)

These are not wired into the build system. Compile individually as shown.

### `test_fips_basic.cc`
Verifies that the OpenSSL FIPS provider loads and that basic digest/encrypt operations succeed under FIPS mode.

```bash
g++ -o /tmp/test_fips_basic tests/manual/test_fips_basic.cc \
    $(pkg-config --cflags --libs openssl) -std=c++17
/tmp/test_fips_basic
```

### `test_keyslot_roundtrip.cc`
Serialises a `KeySlot` struct to protobuf and deserialises it, asserting field equality. Used to catch regressions in the key-slot wire format.

```bash
# Build against the project headers and protobuf
g++ -o /tmp/test_keyslot tests/manual/test_keyslot_roundtrip.cc \
    -I. $(pkg-config --cflags --libs protobuf) -std=c++23
/tmp/test_keyslot
```

### `test_migration.cc`
End-to-end smoke test for V1 → V2 vault migration. Opens a V1 vault from `test_vaults/`, runs the migration path, and asserts that the resulting V2 vault is readable.

```bash
# Requires a full project build first
g++ -o /tmp/test_migration tests/manual/test_migration.cc \
    -I. -Lbuild/src -lkeeptower_core -std=c++23
/tmp/test_migration
```

> See also: `docs/developer/PHASE8_MIGRATION.md` for migration architecture details.

### `minimal_signal_test.cc`
Minimal sigc++ linkage smoke test. Confirms that `sigc::signal` connects and emits correctly. Useful when diagnosing linker or ABI issues with libsigc++.

```bash
g++ -o /tmp/minimal_signal tests/manual/minimal_signal_test.cc \
    $(pkg-config --cflags --libs sigc++-3.0) -std=c++23
/tmp/minimal_signal
```

---

## Shell scripts

Run from the project root.

### `test_serialize_debug.sh`
Creates a temporary vault with a known username and greps the raw vault file to assert that no plaintext username bytes appear on disk. Security regression check for the username-hashing subsystem.

```bash
bash tests/manual/test_serialize_debug.sh
```

### `test_lock_dialog.sh`
Launches KeepTower with `G_MESSAGES_DEBUG=all` and prints a checklist for manually verifying the auto-lock dialog (appears after 60 s of inactivity, accepts correct password, cancels cleanly).

```bash
bash tests/manual/test_lock_dialog.sh
```

### `test_auto_build.sh`
Verifies the automatic OpenSSL build system by backing up any existing `build/openssl-install`, removing `build-test/`, and triggering a clean Meson reconfigure so the OpenSSL fetch-and-build path is exercised.

```bash
bash tests/manual/test_auto_build.sh
```

### `test_preferences.sh`
Smoke-tests Reed-Solomon error-correction preference round-trips (save → reload → verify) using the GSettings schema from `data/`.

```bash
bash tests/manual/test_preferences.sh
```
