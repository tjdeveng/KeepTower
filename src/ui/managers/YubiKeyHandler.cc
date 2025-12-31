/**
 * @file YubiKeyHandler.cc
 * @brief Implementation of YubiKey handler (Phase 5h)
 */

#include "YubiKeyHandler.h"
#include "../../core/VaultManager.h"
#include "../../utils/Log.h"

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../../core/managers/YubiKeyManager.h"
#include "../dialogs/YubiKeyManagerDialog.h"
#endif

#include <gtkmm.h>
#include <format>
#include <span>

namespace UI {

YubiKeyHandler::YubiKeyHandler(Gtk::Window& window, VaultManager* vault_manager)
    : m_window(window)
    , m_vault_manager(vault_manager)
{
}

#ifdef HAVE_YUBIKEY_SUPPORT
void YubiKeyHandler::handle_test() {
    using namespace KeepTower::Log;

    info("Testing YubiKey detection...");

    YubiKeyManager yk_manager{};

    // Initialize YubiKey subsystem
    if (!yk_manager.initialize()) {
        auto dialog = Gtk::AlertDialog::create("YubiKey Initialization Failed");
        dialog->set_detail("Could not initialize YubiKey subsystem. Make sure the required libraries are installed.");
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        error("YubiKey initialization failed");
        return;
    }

    // Test challenge-response functionality FIRST (without calling get_device_info)
    // This avoids any state issues from yk_get_status()
    std::string challenge_result;
    const unsigned char test_challenge[64] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
        0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40
    };

    auto challenge_resp = yk_manager.challenge_response(
        std::span<const unsigned char>(test_challenge, 64),
        true,  // require_touch
        15000  // 15 second timeout
    );

    if (challenge_resp.success) {
        // Now get device info for display
        auto device_info = yk_manager.get_device_info();

        const std::string message = device_info ?
            std::format(
                "YubiKey Test Results\n\n"
                "Serial Number: {}\n"
                "Firmware Version: {}\n"
                "Slot 2 Configured: Yes\n\n"
                "✓ Challenge-Response Working\n"
                "HMAC-SHA1 response received successfully!",
                device_info->serial_number,
                device_info->version_string()
            ) :
            "✓ Challenge-Response Working\nHMAC-SHA1 response received successfully!";

        auto dialog = Gtk::AlertDialog::create("YubiKey Test Passed");
        dialog->set_detail(message);
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        info("YubiKey test passed");
    } else {
        challenge_result = std::format("✗ Challenge-Response Failed\n{}",
                                      challenge_resp.error_message);

        auto dialog = Gtk::AlertDialog::create("YubiKey Test Failed");
        dialog->set_detail(challenge_result);
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        warning("YubiKey challenge-response failed: {}", challenge_resp.error_message);
    }
}

void YubiKeyHandler::handle_manage() {
    // Check if vault is open
    if (!m_vault_manager) {
        auto dialog = Gtk::AlertDialog::create("No Vault Manager");
        dialog->set_detail("Internal error: vault manager not available.");
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        return;
    }

    // Vault manager will check if vault is open
    auto keys = m_vault_manager->get_yubikey_list();

    if (keys.empty()) {
        auto dialog = Gtk::AlertDialog::create("Vault Not YubiKey-Protected");
        dialog->set_detail("This vault does not use YubiKey authentication.");
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        return;
    }

    // Show YubiKey manager dialog (managed by GTK)
    auto* dialog = Gtk::make_managed<YubiKeyManagerDialog>(m_window, m_vault_manager);
    dialog->show();
}
#else
// Stub implementations when YubiKey support is disabled
void YubiKeyHandler::handle_test() {
    auto dialog = Gtk::AlertDialog::create("YubiKey Support Disabled");
    dialog->set_detail("This build of KeepTower was compiled without YubiKey support.");
    dialog->set_buttons({"OK"});
    dialog->choose(m_window, {});
}

void YubiKeyHandler::handle_manage() {
    auto dialog = Gtk::AlertDialog::create("YubiKey Support Disabled");
    dialog->set_detail("This build of KeepTower was compiled without YubiKey support.");
    dialog->set_buttons({"OK"});
    dialog->choose(m_window, {});
}
#endif

} // namespace UI
