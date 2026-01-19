// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef SETTINGS_VALIDATOR_H
#define SETTINGS_VALIDATOR_H

#include <algorithm>
#include <string_view>
#include <giomm/settings.h>
#include "../core/services/UsernameHashService.h"

/**
 * @brief Validates and enforces security constraints on GSettings values
 *
 * This class provides runtime validation to prevent tampering with the
 * GSettings schema file from bypassing security limits. Even if a user
 * modifies the schema file to allow insecure values, these validators
 * will clamp them to safe ranges at runtime.
 *
 * @note This is a static utility class and cannot be instantiated.
 */
class SettingsValidator final {
public:
    // Security constraint constants (C++23: inline static constexpr)
    static inline constexpr int MIN_CLIPBOARD_TIMEOUT{5};      ///< Minimum clipboard clear timeout (5 seconds)
    static inline constexpr int MAX_CLIPBOARD_TIMEOUT{300};    ///< Maximum clipboard clear timeout (5 minutes)
    static inline constexpr int DEFAULT_CLIPBOARD_TIMEOUT{30}; ///< Default clipboard clear timeout (30 seconds)

    static inline constexpr int MIN_AUTO_LOCK_TIMEOUT{60};     ///< Minimum auto-lock timeout (1 minute)
    static inline constexpr int MAX_AUTO_LOCK_TIMEOUT{3600};   ///< Maximum auto-lock timeout (1 hour)
    static inline constexpr int DEFAULT_AUTO_LOCK_TIMEOUT{300}; ///< Default auto-lock timeout (5 minutes)

    static inline constexpr int MIN_PASSWORD_HISTORY{1};       ///< Minimum password history entries
    static inline constexpr int MAX_PASSWORD_HISTORY{20};      ///< Maximum password history entries
    static inline constexpr int DEFAULT_PASSWORD_HISTORY{5};   ///< Default password history entries

    // Username hashing constraints (Phase 2)
    static inline constexpr uint32_t MIN_USERNAME_PBKDF2_ITERATIONS{10000};    ///< Minimum PBKDF2 iterations (NIST SP 800-132)
    static inline constexpr uint32_t MAX_USERNAME_PBKDF2_ITERATIONS{1000000};  ///< Maximum PBKDF2 iterations
    static inline constexpr uint32_t DEFAULT_USERNAME_PBKDF2_ITERATIONS{100000}; ///< Default PBKDF2 iterations

    static inline constexpr uint32_t MIN_USERNAME_ARGON2_MEMORY_KB{8192};     ///< Minimum Argon2 memory (8 MB)
    static inline constexpr uint32_t MAX_USERNAME_ARGON2_MEMORY_KB{1048576};  ///< Maximum Argon2 memory (1 GB)
    static inline constexpr uint32_t DEFAULT_USERNAME_ARGON2_MEMORY_KB{65536}; ///< Default Argon2 memory (64 MB)

    static inline constexpr uint32_t MIN_USERNAME_ARGON2_ITERATIONS{1};       ///< Minimum Argon2 time cost
    static inline constexpr uint32_t MAX_USERNAME_ARGON2_ITERATIONS{10};      ///< Maximum Argon2 time cost
    static inline constexpr uint32_t DEFAULT_USERNAME_ARGON2_ITERATIONS{3};   ///< Default Argon2 time cost

    /**
     * @brief Get clipboard timeout with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated clipboard timeout in seconds (5-300)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_clipboard_timeout(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("clipboard-clear-timeout")};
        return std::clamp(value, MIN_CLIPBOARD_TIMEOUT, MAX_CLIPBOARD_TIMEOUT);
    }

    /**
     * @brief Get auto-lock timeout with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated auto-lock timeout in seconds (60-3600)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_auto_lock_timeout(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("auto-lock-timeout")};
        return std::clamp(value, MIN_AUTO_LOCK_TIMEOUT, MAX_AUTO_LOCK_TIMEOUT);
    }

    /**
     * @brief Get password history limit with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated password history limit (1-20)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static int get_password_history_limit(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const int value{settings->get_int("password-history-limit")};
        return std::clamp(value, MIN_PASSWORD_HISTORY, MAX_PASSWORD_HISTORY);
    }

    /**
     * @brief Check if auto-lock is enabled
     * @param settings GSettings instance (must not be null)
     * @return true if auto-lock is enabled
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static bool is_auto_lock_enabled(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        return settings->get_boolean("auto-lock-enabled");
    }

    /**
     * @brief Check if password history is enabled
     * @param settings GSettings instance (must not be null)
     * @return true if password history tracking is enabled
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static bool is_password_history_enabled(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        return settings->get_boolean("password-history-enabled");
    }

    // ========================================================================
    // Username Hashing Preferences (Phase 2)
    // ========================================================================

    /**
     * @brief Get username hashing algorithm with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated username hash algorithm
     * @note In FIPS mode, returns SHA3_256 if user selected non-FIPS algorithm
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static KeepTower::UsernameHashService::Algorithm
    get_username_hash_algorithm(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const std::string algo_str = settings->get_string("username-hash-algorithm");
        auto algorithm = parse_username_hash_algorithm(algo_str);

        // FIPS mode enforcement: block non-approved algorithms
        if (is_fips_mode_enabled(settings) && !KeepTower::UsernameHashService::is_fips_approved(algorithm)) {
            // Fallback to SHA3-256 (FIPS-approved, recommended)
            return KeepTower::UsernameHashService::Algorithm::SHA3_256;
        }

        return algorithm;
    }

    /**
     * @brief Get PBKDF2 iterations for username hashing with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated iteration count (1000-100000)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static uint32_t get_username_pbkdf2_iterations(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const uint32_t value = settings->get_uint("username-pbkdf2-iterations");
        return std::clamp(value, MIN_USERNAME_PBKDF2_ITERATIONS, MAX_USERNAME_PBKDF2_ITERATIONS);
    }

    /**
     * @brief Get Argon2 memory cost with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated memory cost in KB (8192-1048576)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static uint32_t get_username_argon2_memory_kb(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const uint32_t value = settings->get_uint("username-argon2-memory-kb");
        return std::clamp(value, MIN_USERNAME_ARGON2_MEMORY_KB, MAX_USERNAME_ARGON2_MEMORY_KB);
    }

    /**
     * @brief Get Argon2 time cost with validation
     * @param settings GSettings instance (must not be null)
     * @return Validated time cost iterations (1-10)
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static uint32_t get_username_argon2_iterations(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        const uint32_t value = settings->get_uint("username-argon2-iterations");
        return std::clamp(value, MIN_USERNAME_ARGON2_ITERATIONS, MAX_USERNAME_ARGON2_ITERATIONS);
    }

    /**
     * @brief Check if FIPS mode is enabled
     * @param settings GSettings instance (must not be null)
     * @return true if FIPS-140-3 mode is enabled
     * @note Thread-safe as it only reads from GSettings
     */
    [[nodiscard]] static bool is_fips_mode_enabled(const Glib::RefPtr<Gio::Settings>& settings) noexcept {
        return settings->get_boolean("fips-mode-enabled");
    }

    /**
     * @brief Convert algorithm string to enum
     * @param algo_str Algorithm string from GSettings
     * @return Corresponding Algorithm enum value
     * @note Returns PLAINTEXT_LEGACY for invalid/unknown strings
     */
    [[nodiscard]] static KeepTower::UsernameHashService::Algorithm
    parse_username_hash_algorithm(std::string_view algo_str) noexcept {
        using Algorithm = KeepTower::UsernameHashService::Algorithm;

        if (algo_str == "sha3-256") return Algorithm::SHA3_256;
        if (algo_str == "sha3-384") return Algorithm::SHA3_384;
        if (algo_str == "sha3-512") return Algorithm::SHA3_512;
        if (algo_str == "pbkdf2-sha256") return Algorithm::PBKDF2_SHA256;
        if (algo_str == "argon2id") return Algorithm::ARGON2ID;

        // Default to plaintext for legacy/unknown values
        return Algorithm::PLAINTEXT_LEGACY;
    }

    /**
     * @brief Convert algorithm enum to string
     * @param algorithm Algorithm enum value
     * @return Corresponding GSettings string
     */
    [[nodiscard]] static constexpr std::string_view
    algorithm_to_string(KeepTower::UsernameHashService::Algorithm algorithm) noexcept {
        using Algorithm = KeepTower::UsernameHashService::Algorithm;

        switch (algorithm) {
            case Algorithm::SHA3_256: return "sha3-256";
            case Algorithm::SHA3_384: return "sha3-384";
            case Algorithm::SHA3_512: return "sha3-512";
            case Algorithm::PBKDF2_SHA256: return "pbkdf2-sha256";
            case Algorithm::ARGON2ID: return "argon2id";
            case Algorithm::PLAINTEXT_LEGACY: return "plaintext";
            default: return "plaintext";
        }
    }

private:
    SettingsValidator() = delete;                                    // No instantiation
    ~SettingsValidator() = delete;                                   // No destruction
    SettingsValidator(const SettingsValidator&) = delete;            // No copy
    SettingsValidator& operator=(const SettingsValidator&) = delete; // No copy assignment
    SettingsValidator(SettingsValidator&&) = delete;                 // No move
    SettingsValidator& operator=(SettingsValidator&&) = delete;      // No move assignment
};

#endif // SETTINGS_VALIDATOR_H
