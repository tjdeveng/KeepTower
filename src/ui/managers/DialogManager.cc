// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "DialogManager.h"
#include "../../core/VaultManager.h"
#include "../dialogs/PreferencesDialog.h"
#include "../dialogs/CreatePasswordDialog.h"
#include "../dialogs/PasswordDialog.h"
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../dialogs/VaultMigrationDialog.h"

#include "../../utils/FileDialogExtension.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace UI {

DialogManager::DialogManager(Gtk::Window& parent, VaultManager* vault_manager)
    : m_parent(parent)
    , m_vault_manager(vault_manager) {
}

void DialogManager::configure_dialog(Gtk::Window& dialog) {
    dialog.set_transient_for(m_parent);
    dialog.set_modal(true);
}

void DialogManager::add_file_filters(
    Gtk::FileChooser& chooser,
    const std::vector<std::pair<std::string, std::string>>& filters) {

    for (const auto& [name, pattern] : filters) {
        auto filter = Gtk::FileFilter::create();
        filter->set_name(name);
        filter->add_pattern(pattern);
        chooser.add_filter(filter);
    }
}

void DialogManager::show_error_dialog(const std::string& message, const std::string& title) {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_parent,
        title,
        false,  // use_markup
        Gtk::MessageType::ERROR,
        Gtk::ButtonsType::OK,
        true    // modal
    );
    dialog->set_secondary_text(message);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_info_dialog(const std::string& message, const std::string& title) {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_parent,
        title,
        false,  // use_markup
        Gtk::MessageType::INFO,
        Gtk::ButtonsType::OK,
        true    // modal
    );
    dialog->set_secondary_text(message);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_warning_dialog(const std::string& message, const std::string& title) {
    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_parent,
        title,
        false,  // use_markup
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::OK,
        true    // modal
    );
    dialog->set_secondary_text(message);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_confirmation_dialog(
    const std::string& message,
    const std::string& title,
    const std::function<void(bool)>& callback) {

    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_parent,
        title,
        false,  // use_markup
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true    // modal
    );
    dialog->set_secondary_text(message);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog, callback](int response) {
        bool result = (response == static_cast<int>(Gtk::ResponseType::YES));
        callback(result);
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_open_file_dialog(
    const std::string& title,
    const std::function<void(std::string)>& callback,
    const std::vector<std::pair<std::string, std::string>>& filters) {

    auto* dialog = Gtk::make_managed<Gtk::FileChooserDialog>(
        m_parent,
        title,
        Gtk::FileChooser::Action::OPEN
    );
    configure_dialog(*dialog);

    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Open", Gtk::ResponseType::OK);

    add_file_filters(*dialog, filters);

    dialog->signal_response().connect([dialog, callback](int response) {
        if (response == static_cast<int>(Gtk::ResponseType::OK)) {
            auto file = dialog->get_file();
            if (file) {
                std::string result = file->get_path();
                callback(result);
            }
        }
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_save_file_dialog(
    const std::string& title,
    const std::string& suggested_name,
    const std::function<void(std::string)>& callback,
    const std::vector<std::pair<std::string, std::string>>& filters) {

    auto* dialog = Gtk::make_managed<Gtk::FileChooserDialog>(
        m_parent,
        title,
        Gtk::FileChooser::Action::SAVE
    );
    configure_dialog(*dialog);

    dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
    dialog->add_button("_Save", Gtk::ResponseType::OK);

    if (!suggested_name.empty()) {
        dialog->set_current_name(suggested_name);
    }

    // Build filters locally so we can map the selected filter to its extension.
    std::vector<std::pair<Glib::RefPtr<Gtk::FileFilter>, std::string>> filter_exts;
    filter_exts.reserve(filters.size());

    std::vector<std::string> known_exts;
    known_exts.reserve(filters.size());

    for (const auto& [name, pattern] : filters) {
        auto filter = Gtk::FileFilter::create();
        filter->set_name(name);
        filter->add_pattern(pattern);
        dialog->add_filter(filter);

        auto ext = KeepTower::FileDialogs::ext_from_glob_pattern(pattern);
        if (!ext.empty()) {
            known_exts.push_back(ext);
        }
        filter_exts.emplace_back(std::move(filter), std::move(ext));
    }

    std::sort(known_exts.begin(), known_exts.end());
    known_exts.erase(std::unique(known_exts.begin(), known_exts.end()), known_exts.end());

    const auto desired_ext_for_current_filter = [dialog, filter_exts]() -> std::string {
        const auto current = dialog->get_filter();
        if (!current) {
            return {};
        }

        for (const auto& [f, ext] : filter_exts) {
            if (f == current) {
                return ext;
            }
        }
        return {};
    };

    // Keep the visible filename extension in sync when the user switches export formats.
    dialog->property_filter().signal_changed().connect(
        [dialog, suggested_name, known_exts, desired_ext_for_current_filter]() {
            const auto desired_ext = desired_ext_for_current_filter();
            if (desired_ext.empty()) {
                return;  // e.g. "All files"
            }

            std::string current_name;
            try {
                current_name = dialog->get_current_name().raw();
            } catch (...) {
                current_name.clear();
            }
            if (current_name.empty()) {
                current_name = suggested_name;
            }

            const auto updated = KeepTower::FileDialogs::ensure_filename_extension(
                current_name, desired_ext, known_exts);
            if (updated != current_name && !updated.empty()) {
                dialog->set_current_name(updated);
            }
        });

    dialog->signal_response().connect([dialog, callback, known_exts, desired_ext_for_current_filter](int response) {
        if (response == static_cast<int>(Gtk::ResponseType::OK)) {
            auto file = dialog->get_file();
            if (file) {
                std::string result = file->get_path();
                const auto desired_ext = desired_ext_for_current_filter();
                result = KeepTower::FileDialogs::ensure_path_extension(
                    std::move(result), desired_ext, known_exts);
                callback(result);
            }
        }
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_create_password_dialog(const std::function<void(std::string)>& callback) {
    auto* dialog = Gtk::make_managed<CreatePasswordDialog>(m_parent);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog, callback](int response) {
        std::string password;
        if (response == static_cast<int>(Gtk::ResponseType::OK)) {
            password = dialog->get_password();
        }
        callback(password);
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_password_dialog(const std::function<void(std::string)>& callback) {
    auto* dialog = Gtk::make_managed<PasswordDialog>(m_parent);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog, callback](int response) {
        std::string password;
        if (response == static_cast<int>(Gtk::ResponseType::OK)) {
            password = dialog->get_password();
        }
        callback(password);
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_yubikey_prompt_dialog(
    const std::string& message,
    const std::function<void(bool)>& callback) {

    // Parse message to determine prompt type (INSERT or TOUCH)
    YubiKeyPromptDialog::PromptType type = YubiKeyPromptDialog::PromptType::TOUCH;
    if (message.find("insert") != std::string::npos ||
        message.find("Insert") != std::string::npos) {
        type = YubiKeyPromptDialog::PromptType::INSERT;
    }

    auto* dialog = Gtk::make_managed<YubiKeyPromptDialog>(m_parent, type, "");
    configure_dialog(*dialog);

    dialog->signal_response().connect([=](int response) {
        bool success = (response == static_cast<int>(Gtk::ResponseType::OK));
        callback(success);
        dialog->hide();
    });


    dialog->signal_response().connect([dialog, callback](int response) {
        bool migrated = (response == static_cast<int>(Gtk::ResponseType::OK));
        callback(migrated);
        dialog->hide();
    });

    dialog->present();
}

void DialogManager::show_validation_error(
    const std::string& field_name,
    const std::string& error_details) {

    auto* dialog = Gtk::make_managed<Gtk::MessageDialog>(
        m_parent,
        "Validation Error",
        false,
        Gtk::MessageType::ERROR,
        Gtk::ButtonsType::OK,
        true
    );

    std::string message = "The field '" + field_name + "' contains invalid data.\n\n" + error_details;
    dialog->set_secondary_text(message);
    configure_dialog(*dialog);

    dialog->signal_response().connect([dialog](int) {
        dialog->hide();
    });

    dialog->present();
}

} // namespace UI
