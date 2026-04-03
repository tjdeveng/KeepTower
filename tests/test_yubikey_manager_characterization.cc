// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

/**
 * @file test_yubikey_manager_characterization.cc
 * @brief Characterization tests for YubiKeyManager (Issue #7 refactor safety net)
 *
 * These tests intentionally lock in the current public API behavior and
 * error-message contracts for non-hardware paths. They are designed to run
 * headless and without requiring a connected YubiKey.
 */

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>

#include "lib/yubikey/YubiKeyManager.h"

namespace {

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const char* value) : m_name(name) {
        const char* existing = std::getenv(name);
        if (existing) {
            m_had_previous = true;
            m_previous_value = existing;
        }
        setenv(name, value, 1);
    }

    ~ScopedEnvVar() {
        if (m_had_previous) {
            setenv(m_name.c_str(), m_previous_value.c_str(), 1);
        } else {
            unsetenv(m_name.c_str());
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

private:
    std::string m_name;
    bool m_had_previous{false};
    std::string m_previous_value;
};

} // namespace

TEST(YubiKeyManagerCharacterization, GetDeviceInfo_NotInitialized_SetsExpectedError) {
    YubiKeyManager manager;
    auto info = manager.get_device_info();
    EXPECT_FALSE(info.has_value());

#ifdef HAVE_YUBIKEY_SUPPORT
    EXPECT_EQ(manager.get_last_error(), "YubiKey subsystem not initialized");
#else
    EXPECT_EQ(manager.get_last_error(), "YubiKey support not compiled in");
#endif
}

TEST(YubiKeyManagerCharacterization, IsYubiKeyPresent_DisabledViaEnvVar_ReturnsFalse) {
    ScopedEnvVar disable_detect{"DISABLE_YUBIKEY_DETECT", "1"};

    YubiKeyManager manager;

#ifdef HAVE_YUBIKEY_SUPPORT
    ASSERT_TRUE(manager.initialize(false));
#else
    ASSERT_FALSE(manager.initialize(false));
#endif

    EXPECT_FALSE(manager.is_yubikey_present());
}

TEST(YubiKeyManagerCharacterization, Initialize_FipsFlagIsObservable) {
    YubiKeyManager manager;

#ifdef HAVE_YUBIKEY_SUPPORT
    ASSERT_TRUE(manager.initialize(true));
    EXPECT_TRUE(manager.is_fips_enforced());
#else
    ASSERT_FALSE(manager.initialize(true));
    EXPECT_FALSE(manager.is_fips_enforced());
#endif
}

#ifdef HAVE_YUBIKEY_SUPPORT

TEST(YubiKeyManagerCharacterization, ChallengeResponse_EmptyChallenge_FailsWithExpectedError) {
    ScopedEnvVar disable_detect{"DISABLE_YUBIKEY_DETECT", "1"};

    YubiKeyManager manager;
    ASSERT_TRUE(manager.initialize(false));

    const std::array<unsigned char, 1> dummy{};
    auto result = manager.challenge_response(std::span<const unsigned char>{dummy.data(), 0});

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message,
              "Invalid challenge size: 0 (must be 1-64 bytes)");
    EXPECT_EQ(manager.get_last_error(), result.error_message);
}

TEST(YubiKeyManagerCharacterization, ChallengeResponse_UnsupportedAlgorithm_FailsWithExpectedError) {
    ScopedEnvVar disable_detect{"DISABLE_YUBIKEY_DETECT", "1"};

    YubiKeyManager manager;
    ASSERT_TRUE(manager.initialize(false));

    const std::array<unsigned char, 1> challenge{0x01};
    auto result = manager.challenge_response(
        std::span<const unsigned char>{challenge.data(), challenge.size()},
        YubiKeyAlgorithm::HMAC_SHA512);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_message,
              "Algorithm HMAC-SHA512 not supported. FIDO2 hmac-secret only supports HMAC-SHA256.");
    EXPECT_EQ(manager.get_last_error(), result.error_message);
}

#endif
