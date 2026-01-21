// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <gtkmm.h>
#include <giomm/settings.h>

// Forward declaration
class VaultManager;

/**
 * @brief Preferences dialog for application settings
 *
 * Provides UI for configuring application preferences including
 * Reed-Solomon error correction settings.
 */
class PreferencesDialog final : public Gtk::Dialog {
public:
    /** @brief Construct preferences dialog
     *  @param parent Parent window for modal dialog
     *  @param vault_manager Optional VaultManager for Reed-Solomon settings (can be nullptr) */
    explicit PreferencesDialog(Gtk::Window& parent, VaultManager* vault_manager = nullptr);
    ~PreferencesDialog() override = default;

    // Prevent copying and moving
    PreferencesDialog(const PreferencesDialog&) = delete;
    PreferencesDialog& operator=(const PreferencesDialog&) = delete;
    PreferencesDialog(PreferencesDialog&&) = delete;
    PreferencesDialog& operator=(PreferencesDialog&&) = delete;

private:
    void setup_ui();                                     ///< Initialize main dialog layout with sidebar and stack
    void setup_appearance_page();                        ///< Build appearance preferences page (color scheme)
    void setup_account_security_page();                  ///< Build account security page (clipboard, undo/redo)
    void setup_vault_security_page();                    ///< Build vault security page (auto-lock, FIPS, password history)
    void setup_storage_page();                           ///< Build storage preferences page (FEC, backups)
    void load_settings();                                ///< Load all settings from GSettings into UI controls
    void save_settings();                                ///< Save all UI control values to GSettings
    void apply_color_scheme(const Glib::ustring& scheme);  ///< Apply color scheme to GTK application
    void update_vault_password_history_ui() noexcept;    ///< Update vault password history UI when vault changes
    void update_username_hash_info() noexcept;           ///< Update username hash info label based on selected algorithm
    void update_username_hash_advanced_params() noexcept; ///< Show/hide advanced parameters based on selected algorithm
    void update_argon2_performance_warning() noexcept;   ///< Update Argon2id performance warning based on parameters
    void update_current_vault_kek_info() noexcept;       ///< Update current vault KEK algorithm display
    void update_security_layout() noexcept;              ///< Update two-column layout visibility based on vault state
    void resize_to_content() noexcept;                   ///< Resize dialog to fit content (shrink when content hidden)
    void on_dialog_shown() noexcept;                     ///< Handle dialog shown event (lazy loading)
    void on_rs_enabled_toggled() noexcept;               ///< Handle Reed-Solomon enabled checkbox toggle
    void on_backup_enabled_toggled() noexcept;           ///< Handle backup enabled checkbox toggle
    void on_auto_lock_enabled_toggled() noexcept;        ///< Handle auto-lock enabled checkbox toggle
    void on_account_password_history_toggled() noexcept; ///< Handle account password history checkbox toggle
    void on_undo_redo_enabled_toggled() noexcept;        ///< Handle undo/redo enabled checkbox toggle
    void on_username_hash_changed() noexcept;            ///< Handle username hash algorithm dropdown change
    void on_apply_to_current_toggled() noexcept;         ///< Handle "Apply to current vault" checkbox toggle
    void on_color_scheme_changed() noexcept;             ///< Handle color scheme dropdown selection change
    void on_clear_password_history_clicked() noexcept;   ///< Handle clear password history button click
    void on_backup_path_browse();                        ///< Handle backup path browse button click
    void on_restore_backup();                            ///< Handle restore from backup button click
    void on_response(int response_id) noexcept override; ///< Handle dialog response (Apply/Cancel)

    // Constants
    static constexpr int MIN_REDUNDANCY = 5;
    static constexpr int MAX_REDUNDANCY = 50;
    static constexpr int DEFAULT_REDUNDANCY = 10;
    static constexpr int MIN_BACKUP_COUNT = 1;
    static constexpr int MAX_BACKUP_COUNT = 50;
    static constexpr int DEFAULT_BACKUP_COUNT = 5;
    static constexpr int MIN_CLIPBOARD_TIMEOUT = 5;
    static constexpr int MAX_CLIPBOARD_TIMEOUT = 300;
    static constexpr int DEFAULT_CLIPBOARD_TIMEOUT = 30;
    static constexpr int MIN_AUTO_LOCK_TIMEOUT = 60;
    static constexpr int MAX_AUTO_LOCK_TIMEOUT = 3600;
    static constexpr int DEFAULT_AUTO_LOCK_TIMEOUT = 300;
    static constexpr int MIN_PASSWORD_HISTORY_LIMIT = 0;
    static constexpr int MAX_PASSWORD_HISTORY_LIMIT = 24;
    static constexpr int DEFAULT_PASSWORD_HISTORY_LIMIT = 5;
    static constexpr int DEFAULT_WIDTH = 650;
    static constexpr int DEFAULT_HEIGHT = 500;

    // Settings
    Glib::RefPtr<Gio::Settings> m_settings;
    VaultManager* m_vault_manager;  // Non-owning pointer
    bool m_history_ui_loaded = false;  // Track if vault password history UI has been lazily loaded

    // Main layout widgets
    Gtk::Box m_main_box;
    Gtk::StackSidebar m_sidebar;
    Gtk::Stack m_stack;

    // Appearance page
    Gtk::Box m_appearance_box;
    Gtk::Box m_color_scheme_box;
    Gtk::Label m_color_scheme_label;
    Gtk::DropDown m_color_scheme_dropdown;

    // Account Security page (user's local behavior)
    Gtk::Box m_account_security_box;
    Gtk::Box m_clipboard_timeout_box;
    Gtk::Label m_clipboard_timeout_label;
    Gtk::SpinButton m_clipboard_timeout_spin;
    Gtk::Label m_clipboard_timeout_suffix;
    Gtk::CheckButton m_account_password_history_check;  // Account password reuse detection
    Gtk::Box m_account_password_history_limit_box;
    Gtk::Label m_account_password_history_limit_label;
    Gtk::SpinButton m_account_password_history_limit_spin;
    Gtk::Label m_account_password_history_limit_suffix;
    Gtk::CheckButton m_undo_redo_enabled_check;
    Gtk::Box m_undo_history_limit_box;
    Gtk::Label m_undo_history_limit_label;
    Gtk::SpinButton m_undo_history_limit_spin;
    Gtk::Label m_undo_history_limit_suffix;
    Gtk::Label m_undo_redo_warning;

    // Vault Security page (vault data and policy)
    Gtk::Box m_vault_security_box;
    Gtk::CheckButton m_auto_lock_enabled_check;
    Gtk::Box m_auto_lock_timeout_box;
    Gtk::Label m_auto_lock_timeout_label;
    Gtk::SpinButton m_auto_lock_timeout_spin;
    Gtk::Label m_auto_lock_timeout_suffix;

    // Vault password history UI (only shown when vault open)
    Gtk::Box m_vault_password_history_box;
    Gtk::Label m_vault_policy_label;        // "Current vault policy: 5 passwords"
    Gtk::Label m_current_user_label;         // "Logged in as: alice"
    Gtk::Label m_history_count_label;        // "Password history: 3 entries"
    Gtk::Button m_clear_history_button;      // "Clear My Password History"
    Gtk::Label m_clear_history_warning;      // Warning about clearing

    // Vault user password history default (only shown when no vault open)
    Gtk::Box m_vault_password_history_default_box;
    Gtk::Label m_vault_password_history_default_label;
    Gtk::SpinButton m_vault_password_history_default_spin;
    Gtk::Label m_vault_password_history_default_suffix;
    Gtk::Label m_vault_password_history_default_help;

    /** @name FIPS-140-3 UI Widgets
     * @brief User interface controls for FIPS mode configuration
     *
     * These widgets allow users to enable/disable FIPS-140-3 compliant
     * cryptographic operations and view FIPS provider availability status.
     *
     * **UI Layout in Security Page:**
     * ```
     * FIPS-140-3 Compliance
     * Use FIPS-140-3 validated cryptographic operations
     *
     * [ ] Enable FIPS-140-3 mode (requires restart)
     * ✓ FIPS module available and ready
     * ⚠️  Changes require application restart to take effect
     * ```
     *
     * **Dynamic Behavior:**
     * - Checkbox enabled when FIPS provider available
     * - Checkbox disabled (grayed) when FIPS provider unavailable
     * - Status label shows availability with icon (✓ or ⚠️)
     * - Restart warning always visible when FIPS section present
     *
     * @see setup_security_page() for widget initialization and layout
     * @see load_settings() for reading FIPS preference from GSettings
     * @see save_settings() for persisting FIPS preference to GSettings
     * @{
     */

    /**
     * @brief Checkbox to enable/disable FIPS-140-3 mode
     *
     * **Widget Properties:**
     * - Label: "Enable FIPS-140-3 mode (requires restart)"
     * - State: Active when FIPS mode enabled in settings
     * - Sensitivity: Enabled only when VaultManager::is_fips_available() returns true
     *
     * **User Interaction:**
     * - Checking enables FIPS mode (saved to GSettings on Apply)
     * - Unchecking disables FIPS mode
     * - Changes require application restart to take full effect
     *
     * **Automatic Disabling:**
     * If FIPS provider not available (module not installed or misconfigured),
     * this checkbox is automatically disabled in setup_security_page():
     * @code
     * if (!VaultManager::is_fips_available()) {
     *     m_fips_mode_check.set_sensitive(false);
     * }
     * @endcode
     *
     * @note Disabled state prevents users from enabling unsupported mode
     * @see m_fips_status_label for availability explanation
     */
    Gtk::CheckButton m_fips_mode_check;

    /**
     * @brief Label showing FIPS provider availability status
     *
     * **Display States:**
     * - "✓ FIPS module available and ready" (green/default color)
     * - "⚠️  FIPS module not available" (gray/insensitive color)
     *
     * **Purpose:**
     * Provides immediate visual feedback about FIPS provider status so users
     * understand why the checkbox might be disabled. Prevents confusion when
     * FIPS mode cannot be enabled due to missing FIPS module.
     *
     * **Styling:**
     * Uses Pango markup for icons:
     * @code
     * m_fips_status_label.set_markup("✓ FIPS module available");
     * @endcode
     *
     * **Runtime Detection:**
     * Status determined by calling VaultManager::is_fips_available() during
     * preferences dialog construction:
     * @code
     * if (VaultManager::is_fips_available()) {
     *     m_fips_status_label.set_markup("✓ FIPS module available and ready");
     * } else {
     *     m_fips_status_label.set_markup("⚠️  FIPS module not available");
     * }
     * @endcode
     *
     * @note Status is determined once at dialog creation (not dynamic)
     * @note FIPS availability doesn't change during application runtime
     * @see VaultManager::is_fips_available() for availability logic
     */
    Gtk::Label m_fips_status_label;

    /**
     * @brief Warning label about restart requirement
     *
     * **Display Text:**
     * "⚠️  Changes require application restart to take effect"
     *
     * **Purpose:**
     * Reminds users that FIPS mode changes don't take full effect until the
     * application is restarted. While set_fips_mode() performs runtime switching,
     * restart is recommended for consistent cryptographic provider state across
     * all contexts.
     *
     * **Visibility:**
     * Always visible when FIPS section is present. Not dependent on FIPS
     * availability or enabled state (users need to see this before making changes).
     *
     * **Styling:**
     * Uses style class "dim-label" for subtle appearance:
     * @code
     * m_fips_restart_warning.set_text("⚠️  Changes require application restart...");
     * m_fips_restart_warning.add_css_class("dim-label");
     * @endcode
     *
     * @note Restart requirement applies to both enabling and disabling FIPS mode
     * @note Warning remains visible even when checkbox is disabled
     * @see VaultManager::set_fips_mode() for runtime switching limitations
     */
    Gtk::Label m_fips_restart_warning;

    /** @} */ // end of FIPS-140-3 UI widgets

    /** @name Username Hashing UI Widgets
     * @brief User interface controls for username hashing algorithm selection
     *
     * These widgets allow users to select the hashing algorithm used for
     * usernames in newly created vaults. This affects security and FIPS compliance.
     *
     * **Available Algorithms:**
     * - plaintext (DEPRECATED, legacy only)
     * - sha3-256 (FIPS-approved, default, 32 bytes)
     * - sha3-384 (FIPS-approved, 48 bytes)
     * - sha3-512 (FIPS-approved, 64 bytes)
     * - pbkdf2-sha256 (FIPS-approved, 32 bytes, slow)
     * - argon2id (non-FIPS, 32 bytes, very slow, requires ENABLE_ARGON2)
     *
     * **FIPS Enforcement:**
     * When FIPS mode is enabled, only FIPS-approved algorithms are available.
     * The dropdown will be filtered to show only sha3-* and pbkdf2-sha256.
     *
     * **UI Layout in Security Page:**
     * ```
     * Username Hashing Algorithm (New Vaults Only)
     * Choose how usernames are stored in newly created vault files
     *
     * Algorithm: [SHA3-256 (FIPS)       v]
     * ℹ️  Fast, FIPS-approved, 32-byte hash. Recommended for most users.
     * ```
     *
     * @{
     */

    /**
     * @brief Dropdown for selecting username hashing algorithm
     *
     * Contains entries for all available algorithms with FIPS indicators.
     * Filtered dynamically based on FIPS mode and build configuration.
     */
    Gtk::ComboBoxText m_username_hash_combo;

    /**
     * @brief Info label showing details about selected algorithm
     *
     * Dynamically updates to show:
     * - Hash size (e.g., "32-byte hash")
     * - Performance (e.g., "Fast", "Slow", "Very slow")
     * - FIPS compliance status
     * - Security trade-offs and recommendations
     */
    Gtk::Label m_username_hash_info;

    /**
     * @brief Advanced parameters section (shown/hidden based on algorithm)
     *
     * Contains algorithm-specific tuning parameters:
     * - PBKDF2: Iteration count (10,000-1,000,000)
     * - Argon2id: Memory cost (8 MB-1 GB) and time cost (1-10)
     *
     * Managed pointers ensure proper GTK4 widget lifecycle management.
     */
    Gtk::Box* m_username_hash_advanced_box;

    // PBKDF2 advanced parameters (managed pointers)
    Gtk::Box* m_pbkdf2_iterations_box;
    Gtk::SpinButton* m_pbkdf2_iterations_spin;

    // Argon2 advanced parameters (managed pointers)
    Gtk::Box* m_argon2_params_box;
    Gtk::SpinButton* m_argon2_memory_spin;
    Gtk::SpinButton* m_argon2_time_spin;
    Gtk::Label* m_argon2_perf_warning; ///< Performance warning label (managed)

    /** @} */ // end of Username Hashing UI widgets

    // Two-column layout for Vault Security tab
    Gtk::Grid* m_security_grid;       ///< Grid container for two-column layout
    Gtk::Box* m_security_right_column; ///< Right column (current vault info)

    /** @name Current Vault KEK Display (Read-Only)
     * @brief Display current vault's KEK derivation algorithm
     *
     * Shows the cryptographic settings of the currently open vault.
     * Read-only display - algorithms cannot be changed after vault creation.
     *
     * @{
     */

    /**
     * @brief Container for current vault KEK information
     * Shown only when a vault is open, hidden otherwise.
     */
    Gtk::Box* m_current_vault_kek_box;

    /**
     * @brief Shows current vault's username hash algorithm
     * Example: "Username Algorithm: SHA3-256 (FIPS)"
     */
    Gtk::Label m_current_username_hash_label;

    /**
     * @brief Shows current vault's KEK derivation algorithm
     * Example: "Password KEK Algorithm: PBKDF2-HMAC-SHA256 (FIPS)"
     */
    Gtk::Label m_current_kek_label;

    /**
     * @brief Shows algorithm parameters
     * Example: "Parameters: 600,000 iterations"
     */
    Gtk::Label m_current_kek_params_label;

    /** @} */ // end of Current Vault KEK Display

    // Storage page (Reed-Solomon + Backups)
    Gtk::Box m_storage_box;
    Gtk::Label m_rs_section_title;
    Gtk::Label m_rs_description;
    Gtk::CheckButton m_rs_enabled_check;
    Gtk::Box m_redundancy_box;
    Gtk::Label m_redundancy_label;
    Gtk::SpinButton m_redundancy_spin;
    Gtk::Label m_redundancy_suffix;
    Gtk::Label m_redundancy_help;
    Gtk::CheckButton m_apply_to_current_check;
    Gtk::Label m_backup_section_title;
    Gtk::Label m_backup_description;
    Gtk::CheckButton m_backup_enabled_check;
    Gtk::Box m_backup_count_box;
    Gtk::Label m_backup_count_label;
    Gtk::SpinButton m_backup_count_spin;
    Gtk::Label m_backup_count_suffix;
    Gtk::Label m_backup_help;
    Gtk::Box m_backup_path_box;
    Gtk::Label m_backup_path_label;
    Gtk::Entry m_backup_path_entry;
    Gtk::Button m_backup_path_browse_button;
    Gtk::Button m_restore_backup_button;
};

#endif // PREFERENCESDIALOG_H
