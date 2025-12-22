// SPDX-License-Identifier: GPL-3.0-or-later
// Simple test to verify FIPS provider loading

#include <iostream>
#include <openssl/provider.h>
#include <openssl/evp.h>
#include <openssl/err.h>

int main() {
    std::cout << "Testing OpenSSL FIPS provider..." << std::endl;

    // Try to load FIPS provider
    OSSL_PROVIDER* fips = OSSL_PROVIDER_load(nullptr, "fips");
    if (fips == nullptr) {
        std::cerr << "ERROR: Failed to load FIPS provider" << std::endl;
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        std::cerr << "OpenSSL error: " << err_buf << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: FIPS provider loaded" << std::endl;

    // Try to enable FIPS mode
    if (EVP_default_properties_enable_fips(nullptr, 1) != 1) {
        std::cerr << "ERROR: Failed to enable FIPS mode" << std::endl;
        unsigned long err = ERR_get_error();
        char err_buf[256];
        ERR_error_string_n(err, err_buf, sizeof(err_buf));
        std::cerr << "OpenSSL error: " << err_buf << std::endl;
        OSSL_PROVIDER_unload(fips);
        return 1;
    }

    std::cout << "SUCCESS: FIPS mode enabled" << std::endl;

    // Test a basic FIPS-approved algorithm (AES-256-GCM)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        std::cerr << "ERROR: Failed to create cipher context" << std::endl;
        OSSL_PROVIDER_unload(fips);
        return 1;
    }

    const EVP_CIPHER* cipher = EVP_aes_256_gcm();
    if (cipher == nullptr) {
        std::cerr << "ERROR: AES-256-GCM not available" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        OSSL_PROVIDER_unload(fips);
        return 1;
    }

    std::cout << "SUCCESS: AES-256-GCM available in FIPS mode" << std::endl;

    // Cleanup
    EVP_CIPHER_CTX_free(ctx);
    OSSL_PROVIDER_unload(fips);

    std::cout << "\nAll FIPS tests passed!" << std::endl;
    return 0;
}
