// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#pragma once

#include "config.h"

#include <atomic>
#include <mutex>

namespace KeepTower::YubiKeyInternal {

// NOTE: This helper is intentionally header-only to avoid touching Meson source
// lists across many existing test binaries. It is meant for internal use by
// YubiKeyManager only.

inline std::mutex& fido2_mutex() noexcept {
    static std::mutex mutex;
    return mutex;
}

inline std::atomic<bool>& fido2_initialized_flag() noexcept {
    static std::atomic<bool> initialized{false};
    return initialized;
}

} // namespace KeepTower::YubiKeyInternal
