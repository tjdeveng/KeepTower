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

    // Initialize YubiKey subsystem in FIPS mode (same as vault operations)
    if (!yk_manager.initialize(true)) {
        auto dialog = Gtk::AlertDialog::create("YubiKey Initialization Failed");
        dialog->set_detail("Could not initialize YubiKey subsystem. Make sure the required libraries are installed.");
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        error("YubiKey initialization failed");
        return;
    }

    // Get device info to test detection and capabilities
    // Note: FIDO2 challenge-response requires enrolled credentials, so we only test detection
    auto device_info = yk_manager.get_device_info();

    if (device_info) {
        const std::string message = std::format(
            "YubiKey Detected Successfully\n\n"
            "Serial Number: {}\n"
            "Firmware Version: {}\n"
            "FIDO2 Support: Yes\n"
            "HMAC-Secret Extension: {}\n"
            "FIPS Capable: {}\n"
            "FIPS Mode: {}\n\n"
            "Device is ready for vault operations.\n\n"
            "Note: Challenge-response requires an enrolled\n"
            "credential (created when you set up a vault).",
            device_info->serial_number,
            device_info->version_string(),
            device_info->slot2_configured ? "Yes" : "No",
            device_info->is_fips_capable ? "Yes" : "No",
            device_info->is_fips_mode ? "Yes" : "No"
        );

        auto dialog = Gtk::AlertDialog::create("YubiKey Test Passed");
        dialog->set_detail(message);
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        info("YubiKey test passed: {}, firmware {}",
             device_info->serial_number, device_info->version_string());
    } else {
        auto dialog = Gtk::AlertDialog::create("YubiKey Test Failed");
        dialog->set_detail("Could not detect YubiKey device.\n\n"
                          "Please ensure:\n"
                          "• YubiKey is inserted\n"
                          "• You have permission to access /dev/hidraw*\n"
                          "• libfido2 is properly installed");
        dialog->set_buttons({"OK"});
        dialog->choose(m_window, {});
        warning("YubiKey detection failed");
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
