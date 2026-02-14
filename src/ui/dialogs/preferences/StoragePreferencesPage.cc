// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "StoragePreferencesPage.h"

#include "../../../core/VaultManager.h"
#include "../../../utils/Log.h"
#include "../../../utils/StringHelpers.h"

#include <algorithm>
#include <filesystem>

namespace KeepTower::Ui {

namespace {
constexpr int MIN_REDUNDANCY = 5;
constexpr int MAX_REDUNDANCY = 50;
constexpr int DEFAULT_REDUNDANCY = 10;

constexpr int MIN_BACKUP_COUNT = 1;
constexpr int MAX_BACKUP_COUNT = 50;
constexpr int DEFAULT_BACKUP_COUNT = 5;

}  // namespace

StoragePreferencesPage::StoragePreferencesPage(VaultManager* vault_manager, Glib::RefPtr<Gio::Settings> settings)
        : Gtk::Box(Gtk::Orientation::VERTICAL, 18),
            m_vault_manager(vault_manager),
            m_settings(std::move(settings)),
      m_rs_section_title("<b>Error Correction</b>"),
      m_rs_description("Protect vault files from corruption on unreliable storage"),
      m_rs_enabled_check("Enable Reed-Solomon error correction for new vaults"),
      m_redundancy_box(Gtk::Orientation::HORIZONTAL, 12),
      m_redundancy_label("Redundancy:"),
      m_redundancy_suffix("%"),
      m_redundancy_help("Higher values provide more protection but increase file size"),
      m_apply_to_current_check("Apply to current vault (not defaults)"),
      m_backup_section_title("<b>Automatic Backups</b>"),
      m_backup_description("Create timestamped backups when saving vaults"),
      m_backup_enabled_check("Enable automatic backups"),
      m_backup_count_box(Gtk::Orientation::HORIZONTAL, 12),
      m_backup_count_label("Keep up to:"),
      m_backup_count_suffix(" backups"),
      m_backup_help("Older backups are automatically deleted"),
      m_backup_path_box(Gtk::Orientation::HORIZONTAL, 12),
      m_backup_path_browse_button("Browse..."),
      m_restore_backup_button("Restore from Backup...") {
    set_margin_start(18);
    set_margin_end(18);
    set_margin_top(18);
    set_margin_bottom(18);

    m_info_label = Gtk::make_managed<Gtk::Label>();
    m_info_label->set_halign(Gtk::Align::START);
    m_info_label->set_wrap(true);
    m_info_label->set_max_width_chars(60);
    m_info_label->add_css_class("dim-label");
    m_info_label->set_margin_bottom(12);
    append(*m_info_label);

    // Reed-Solomon section
    auto* rs_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);

    m_rs_section_title.set_use_markup(true);
    m_rs_section_title.set_halign(Gtk::Align::START);
    m_rs_section_title.add_css_class("heading");
    rs_section->append(m_rs_section_title);

    m_rs_description.set_wrap(true);
    m_rs_description.set_max_width_chars(60);
    m_rs_description.set_halign(Gtk::Align::START);
    m_rs_description.add_css_class("dim-label");
    rs_section->append(m_rs_description);

    rs_section->append(m_rs_enabled_check);

    m_redundancy_label.set_halign(Gtk::Align::START);
    m_redundancy_box.append(m_redundancy_label);

    auto adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_REDUNDANCY),
        static_cast<double>(MIN_REDUNDANCY),
        static_cast<double>(MAX_REDUNDANCY),
        1.0, 5.0, 0.0);

    m_redundancy_spin.set_adjustment(adjustment);
    m_redundancy_spin.set_digits(0);
    m_redundancy_spin.set_value(DEFAULT_REDUNDANCY);
    m_redundancy_box.append(m_redundancy_spin);

    m_redundancy_suffix.set_halign(Gtk::Align::START);
    m_redundancy_box.append(m_redundancy_suffix);

    m_redundancy_box.set_halign(Gtk::Align::START);
    rs_section->append(m_redundancy_box);

    m_redundancy_help.set_wrap(true);
    m_redundancy_help.set_max_width_chars(60);
    m_redundancy_help.set_halign(Gtk::Align::START);
    m_redundancy_help.add_css_class("dim-label");
    rs_section->append(m_redundancy_help);

    // Apply to current vault checkbox (only shown when vault is open)
    m_apply_to_current_check.set_margin_top(6);
    rs_section->append(m_apply_to_current_check);

    append(*rs_section);

    // Backup section
    auto* backup_section = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    backup_section->set_margin_top(24);

    m_backup_section_title.set_use_markup(true);
    m_backup_section_title.set_halign(Gtk::Align::START);
    m_backup_section_title.add_css_class("heading");
    backup_section->append(m_backup_section_title);

    m_backup_description.set_wrap(true);
    m_backup_description.set_max_width_chars(60);
    m_backup_description.set_halign(Gtk::Align::START);
    m_backup_description.add_css_class("dim-label");
    backup_section->append(m_backup_description);

    backup_section->append(m_backup_enabled_check);

    m_backup_count_label.set_halign(Gtk::Align::START);
    m_backup_count_box.append(m_backup_count_label);

    auto backup_adjustment = Gtk::Adjustment::create(
        static_cast<double>(DEFAULT_BACKUP_COUNT),
        static_cast<double>(MIN_BACKUP_COUNT),
        static_cast<double>(MAX_BACKUP_COUNT),
        1.0, 5.0, 0.0);

    m_backup_count_spin.set_adjustment(backup_adjustment);
    m_backup_count_spin.set_digits(0);
    m_backup_count_spin.set_value(DEFAULT_BACKUP_COUNT);
    m_backup_count_box.append(m_backup_count_spin);

    m_backup_count_suffix.set_halign(Gtk::Align::START);
    m_backup_count_box.append(m_backup_count_suffix);

    m_backup_count_box.set_halign(Gtk::Align::START);
    backup_section->append(m_backup_count_box);

    m_backup_help.set_wrap(true);
    m_backup_help.set_max_width_chars(60);
    m_backup_help.set_halign(Gtk::Align::START);
    m_backup_help.add_css_class("dim-label");
    backup_section->append(m_backup_help);

    // Backup path controls
    m_backup_path_label.set_text("Backup Directory:");
    m_backup_path_label.set_halign(Gtk::Align::START);
    m_backup_path_box.append(m_backup_path_label);

    m_backup_path_entry.set_placeholder_text("(Leave empty to use vault directory)");
    m_backup_path_entry.set_hexpand(true);
    m_backup_path_box.append(m_backup_path_entry);

    m_backup_path_box.set_spacing(8);
    m_backup_path_box.set_halign(Gtk::Align::FILL);
    m_backup_path_box.append(m_backup_path_browse_button);

    backup_section->append(m_backup_path_box);

    m_restore_backup_button.set_halign(Gtk::Align::START);
    m_restore_backup_button.set_margin_top(12);
    backup_section->append(m_restore_backup_button);

    append(*backup_section);

    // Signals
    m_rs_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &StoragePreferencesPage::on_rs_enabled_toggled));

    m_backup_enabled_check.signal_toggled().connect(
        sigc::mem_fun(*this, &StoragePreferencesPage::on_backup_enabled_toggled));

    m_apply_to_current_check.signal_toggled().connect(
        sigc::mem_fun(*this, &StoragePreferencesPage::on_apply_to_current_toggled));

    m_backup_path_browse_button.signal_clicked().connect(
        sigc::mem_fun(*this, &StoragePreferencesPage::on_backup_path_browse));
    m_restore_backup_button.signal_clicked().connect(
        sigc::mem_fun(*this, &StoragePreferencesPage::on_restore_backup));
}

void StoragePreferencesPage::load_from_model(const PreferencesModel& model) {
    if (m_info_label) {
        if (model.vault_open) {
            m_info_label->set_markup(
                "<span size='small'>ℹ️  Showing settings for the current vault (use checkbox to change defaults for new vaults)</span>");
        } else {
            m_info_label->set_markup("<span size='small'>ℹ️  These settings will be used as defaults for new vaults</span>");
        }
    }

    m_apply_to_current_check.set_visible(model.vault_open);

    m_apply_to_current_check.set_active(model.apply_to_current_vault_fec);

    m_rs_enabled_check.set_active(model.rs_enabled);
    m_redundancy_spin.set_value(std::clamp(model.rs_redundancy_percent, MIN_REDUNDANCY, MAX_REDUNDANCY));

    m_backup_enabled_check.set_active(model.backup_enabled);
    m_backup_count_spin.set_value(std::clamp(model.backup_count, MIN_BACKUP_COUNT, MAX_BACKUP_COUNT));

    m_backup_path_entry.set_text(KeepTower::make_valid_utf8(model.backup_path, "backup_path"));

    on_rs_enabled_toggled();
    on_backup_enabled_toggled();
}

void StoragePreferencesPage::on_apply_to_current_toggled() noexcept {
    if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
        return;
    }

    bool rs_enabled;
    int rs_redundancy;

    if (m_apply_to_current_check.get_active()) {
        rs_enabled = m_vault_manager->is_reed_solomon_enabled();
        rs_redundancy = m_vault_manager->get_rs_redundancy_percent();
    } else {
        if (!m_settings) {
            return;
        }
        rs_enabled = m_settings->get_boolean("use-reed-solomon");
        rs_redundancy = m_settings->get_int("rs-redundancy-percent");
        rs_redundancy = std::clamp(rs_redundancy, MIN_REDUNDANCY, MAX_REDUNDANCY);
    }

    m_rs_enabled_check.set_active(rs_enabled);
    m_redundancy_spin.set_value(rs_redundancy);
}

void StoragePreferencesPage::store_to_model(PreferencesModel& model) const {
    model.apply_to_current_vault_fec = m_apply_to_current_check.get_active();

    model.rs_enabled = m_rs_enabled_check.get_active();
    model.rs_redundancy_percent = std::clamp(static_cast<int>(m_redundancy_spin.get_value()), MIN_REDUNDANCY, MAX_REDUNDANCY);

    model.backup_enabled = m_backup_enabled_check.get_active();
    model.backup_count = std::clamp(static_cast<int>(m_backup_count_spin.get_value()), MIN_BACKUP_COUNT, MAX_BACKUP_COUNT);

    model.backup_path = m_backup_path_entry.get_text();
}

void StoragePreferencesPage::on_rs_enabled_toggled() noexcept {
    const bool enabled = m_rs_enabled_check.get_active();

    m_redundancy_label.set_sensitive(enabled);
    m_redundancy_spin.set_sensitive(enabled);
    m_redundancy_suffix.set_sensitive(enabled);
    m_redundancy_help.set_sensitive(enabled);
}

void StoragePreferencesPage::on_backup_enabled_toggled() noexcept {
    const bool enabled = m_backup_enabled_check.get_active();

    m_backup_count_label.set_sensitive(enabled);
    m_backup_count_spin.set_sensitive(enabled);
    m_backup_count_suffix.set_sensitive(enabled);
    m_backup_help.set_sensitive(enabled);
}

void StoragePreferencesPage::on_backup_path_browse() {
    auto* parent_window = dynamic_cast<Gtk::Window*>(get_root());
    if (!parent_window) {
        return;
    }

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Backup Directory");
    dialog->set_modal(true);

    std::string current_path = m_backup_path_entry.get_text();
    if (!current_path.empty() && std::filesystem::exists(current_path)) {
        auto folder = Gio::File::create_for_path(current_path);
        dialog->set_initial_folder(folder);
    }

    auto slot = [this, dialog](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto folder = dialog->select_folder_finish(result);
            if (folder) {
                m_backup_path_entry.set_text(KeepTower::make_valid_utf8(folder->get_path(), "backup_path"));
            }
        } catch (const Gtk::DialogError& err) {
            if (err.code() != Gtk::DialogError::DISMISSED) {
                KeepTower::Log::warning("File dialog error: {}", err.what());
            }
        } catch (const Glib::Error& err) {
            KeepTower::Log::error("Error selecting backup folder: {}", err.what());
        }
    };

    dialog->select_folder(*parent_window, slot);
}

void StoragePreferencesPage::on_restore_backup() {
    auto* parent_window = dynamic_cast<Gtk::Window*>(get_root());
    if (!parent_window) {
        return;
    }

    if (!m_vault_manager) {
        return;
    }

    if (m_vault_manager->is_vault_open()) {
        auto* error_dialog = new Gtk::MessageDialog(
            *parent_window,
            "Vault Must Be Closed",
            false,
            Gtk::MessageType::ERROR,
            Gtk::ButtonsType::OK,
            true);
        error_dialog->set_secondary_text(
            "Please close the current vault before restoring from a backup.");
        error_dialog->signal_response().connect([error_dialog](int) { delete error_dialog; });
        error_dialog->show();
        return;
    }

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Vault to Restore");
    dialog->set_modal(true);

    auto filter = Gtk::FileFilter::create();
    filter->set_name("Vault Files");
    filter->add_pattern("*.vault");

    auto filter_list = Gio::ListStore<Gtk::FileFilter>::create();
    filter_list->append(filter);
    dialog->set_filters(filter_list);
    dialog->set_default_filter(filter);

    auto slot = [this, dialog, parent_window](const Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (!file) {
                return;
            }

            std::string vault_path = file->get_path();

            auto* confirm_dialog = new Gtk::MessageDialog(
                *parent_window,
                "Confirm Restore",
                false,
                Gtk::MessageType::WARNING,
                Gtk::ButtonsType::OK_CANCEL,
                true);
            confirm_dialog->set_secondary_text(
                "This will replace the current vault file with the most recent backup. "
                "The current vault will be lost unless you have another backup.\n\n"
                "Are you sure you want to continue?");

            confirm_dialog->signal_response().connect([this, confirm_dialog, parent_window, vault_path](int response) {
                if (response == Gtk::ResponseType::OK) {
                    auto result = m_vault_manager->restore_from_most_recent_backup(vault_path);

                    auto* result_dialog = new Gtk::MessageDialog(
                        *parent_window,
                        result ? "Restore Successful" : "Restore Failed",
                        false,
                        result ? Gtk::MessageType::INFO : Gtk::MessageType::ERROR,
                        Gtk::ButtonsType::OK,
                        true);

                    if (result) {
                        result_dialog->set_secondary_text(
                            "The vault has been successfully restored from the most recent backup.");
                    } else {
                        std::string error_msg = "Failed to restore backup: ";
                        error_msg += std::string(KeepTower::to_string(result.error()));
                        result_dialog->set_secondary_text(error_msg);
                    }

                    result_dialog->signal_response().connect([result_dialog](int) { delete result_dialog; });
                    result_dialog->show();
                }
                delete confirm_dialog;
            });
            confirm_dialog->show();

        } catch (const Gtk::DialogError& err) {
            if (err.code() != Gtk::DialogError::DISMISSED) {
                KeepTower::Log::warning("File dialog error: {}", err.what());
            }
        } catch (const Glib::Error& err) {
            KeepTower::Log::error("Error opening file dialog: {}", err.what());
        }
    };

    dialog->open(*parent_window, slot);
}

}  // namespace KeepTower::Ui
