/**
 * @file YubiKeyHandler.h
 * @brief Handler for YubiKey operations (Phase 5h)
 *
 * Extracts YubiKey testing and management functionality from MainWindow.
 */

#ifndef UI_MANAGERS_YUBIKEY_HANDLER_H
#define UI_MANAGERS_YUBIKEY_HANDLER_H

#include <memory>
#include <string>

// Forward declarations
class VaultManager;
namespace Gtk {
    class Window;
}

namespace UI {

/**
 * @class YubiKeyHandler
 * @brief Handles YubiKey testing and management operations
 *
 * Phase 5h extraction: Manages YubiKey-related operations including:
 * - Testing YubiKey detection and challenge-response
 * - Managing YubiKey backup keys for vault
 */
class YubiKeyHandler {
public:
    /**
     * @brief Construct YubiKey handler
     * @param window Parent window for dialogs
     * @param vault_manager Vault manager for YubiKey operations
     */
    YubiKeyHandler(Gtk::Window& window, VaultManager* vault_manager);

    /**
     * @brief Test YubiKey detection and challenge-response
     *
     * Performs complete YubiKey test:
     * 1. Initializes YubiKey subsystem
     * 2. Tests challenge-response (requires touch)
     * 3. Gets device info (serial, firmware)
     * 4. Shows results dialog
     */
    void handle_test();

    /**
     * @brief Manage YubiKey backup keys
     *
     * Opens YubiKeyManagerDialog to:
     * - View registered YubiKeys
     * - Add backup YubiKeys
     * - Remove YubiKeys
     *
     * Requires vault to be open and YubiKey-protected.
     */
    void handle_manage();

private:
    Gtk::Window& m_window;
    VaultManager* m_vault_manager;
};

} // namespace UI

#endif // UI_MANAGERS_YUBIKEY_HANDLER_H
