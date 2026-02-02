# Sanitizer Run Evidence

**Project:** KeepTower

**Date:** 2026-02-01

**Build dir:** `build-asan/`

**Sanitizers:** AddressSanitizer (ASan), UndefinedBehaviorSanitizer (UBSan)

---

## 1. Environment

- **ASAN_OPTIONS:** `halt_on_error=1:abort_on_error=1:print_summary=1`
- **UBSAN_OPTIONS:** `halt_on_error=1:print_stacktrace=1`
- **Meson flags:** `--num-processes 1` (avoid test interference), `--timeout-multiplier 3` (ASan runtime overhead)

---

## 2. Commands Executed

### 2.1 Gate B set (ASan/UBSan)

```
meson test -C build-asan --print-errorlogs --num-processes 1 --timeout-multiplier 3 \
  "FIPS Mode Tests" \
  "Memory Locking Security Tests" \
  vault_crypto \
  "VaultCryptoService Unit Tests" \
  vault_io \
  "VaultFileService Unit Tests" \
  "UI Security Tests" \
  "Settings Validator Tests"
```

**Result:** ✅ All 8 tests passed.

### 2.2 Migration suite (ASan/UBSan)

```
meson test -C build-asan --print-errorlogs --num-processes 1 --timeout-multiplier 3 --suite migration
```

**Result:** ✅ All 4 migration tests passed:
- Username Hash Migration Tests
- Username Hash Migration Priority 2 Tests
- Username Hash Migration Priority 3 Tests
- Username Hash Migration Concurrency Tests

---

## 3. Notes

- The Meson test log is written to `build-asan/meson-logs/testlog.txt`.
- For ASan builds, `tests/meson.build` disables LeakSanitizer leak detection for **only** the `FIPS Mode Tests` binary (via `ASAN_OPTIONS=detect_leaks=0:...`) due to OpenSSL provider/FIPS module process-lifetime allocations that can otherwise abort at exit.
- Compilation during the migration run emitted deprecation warnings related to `keeptower::YubiKeyConfig::serial()` usage via generated protobuf headers; this is tracked as a non-blocking code quality note (not a sanitizer failure).
