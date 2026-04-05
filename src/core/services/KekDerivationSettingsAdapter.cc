// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 tjdeveng

#include "KekDerivationSettingsAdapter.h"

#include "../../utils/Log.h"

namespace KeepTower {

KekDerivationService::Algorithm
KekDerivationSettingsAdapter::get_algorithm(
    const Glib::RefPtr<Gio::Settings>& settings) noexcept {

    if (!settings) {
        Log::warning("KekDerivationSettingsAdapter: null settings, defaulting to PBKDF2");
        return KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;
    }

    if (settings->get_boolean("fips-mode-enabled")) {
        Log::debug("KekDerivationSettingsAdapter: FIPS mode enabled, using PBKDF2");
        return KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;
    }

    Glib::ustring pref = settings->get_string("username-hash-algorithm");

    if (pref == "argon2id") {
        Log::debug("KekDerivationSettingsAdapter: Using Argon2id from settings");
        return KekDerivationService::Algorithm::ARGON2ID;
    }

    if (pref == "pbkdf2") {
        Log::debug("KekDerivationSettingsAdapter: Using PBKDF2 from settings");
        return KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;
    }

    if (pref == "sha3-256" || pref == "sha3-384" || pref == "sha3-512") {
        Log::warning("KekDerivationSettingsAdapter: SHA3 unsuitable for KEK derivation, using PBKDF2 fallback");
        return KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;
    }

    Log::warning("KekDerivationSettingsAdapter: Unknown algorithm '{}', defaulting to PBKDF2",
                 std::string(pref));
    return KekDerivationService::Algorithm::PBKDF2_HMAC_SHA256;
}

KekDerivationService::AlgorithmParameters
KekDerivationSettingsAdapter::get_parameters(
    const Glib::RefPtr<Gio::Settings>& settings) noexcept {

    KekDerivationService::AlgorithmParameters params;

    if (!settings) {
        Log::warning("KekDerivationSettingsAdapter: null settings, using defaults");
        return params;
    }

    params.pbkdf2_iterations = settings->get_uint("username-pbkdf2-iterations");
    params.argon2_memory_kb = settings->get_uint("username-argon2-memory-kb");
    params.argon2_time_cost = settings->get_uint("username-argon2-iterations");
    params.argon2_parallelism = 4;

    Log::debug("KekDerivationSettingsAdapter: Parameters from settings - "
               "PBKDF2: {} iterations, Argon2: {} KB / {} iterations / {} threads",
               params.pbkdf2_iterations,
               params.argon2_memory_kb,
               params.argon2_time_cost,
               params.argon2_parallelism);
    return params;
}

} // namespace KeepTower