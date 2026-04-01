// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

#include "../../core/VaultManager.h"
#include "VaultIOHandler.h"
#include "DialogManager.h"
#include "../../utils/Log.h"
#include "../../utils/StringHelpers.h"
#include "../../utils/ImportExport.h"
#include "../windows/MainWindow.h"
#include "../dialogs/PasswordDialog.h"

// Forward declare OPENSSL_cleanse to avoid OpenSSL UI type conflict with our UI namespace
extern "C" {
    void OPENSSL_cleanse(void *ptr, size_t len);
}

#ifdef HAVE_YUBIKEY_SUPPORT
#include "../dialogs/YubiKeyPromptDialog.h"
#include "../../core/managers/YubiKeyManager.h"
#endif

#include <algorithm>
#include <format>
#include <utility>

namespace UI {

VaultIOHandler::VaultIOHandler(MainWindow& window,
                               VaultManager* vault_manager,
                               DialogManager* dialog_manager)
    : m_window(window)
    , m_vault_manager(vault_manager)
    , m_dialog_manager(dialog_manager)
    , m_import_flow(Flows::ImportFlowController::Ports{
          Flows::MessagePort{
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_info_dialog(message, title);
              },
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_warning_dialog(message, title);
              },
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_error_dialog(message, title);
              },
          },
          Flows::FileDialogPort{
              [this](const std::string& title,
                     const Flows::FileDialogPort::Filters& filters,
                     std::function<void(const std::string&)> cb) {
                  m_dialog_manager->show_open_file_dialog(
                      title,
                      [cb = std::move(cb)](std::string path) {
                          cb(path);
                      },
                      filters);
              },
              {},
          },
          Flows::ImportOperationPort{
              [this](const std::string& source_path)
                  -> std::expected<Flows::ImportOperationPort::Summary, std::string> {
                  std::expected<std::vector<keeptower::AccountRecord>, ImportExport::ImportError> result;
                  std::string format_name;

                  if (source_path.ends_with(".xml")) {
                      result = ImportExport::import_from_keepass_xml(source_path);
                      format_name = "KeePass XML";
                  } else if (source_path.ends_with(".1pif")) {
                      result = ImportExport::import_from_1password(source_path);
                      format_name = "1Password 1PIF";
                  } else {
                      result = ImportExport::import_from_csv(source_path);
                      format_name = "CSV";
                  }

                  if (!result.has_value()) {
                      const char* error_msg = nullptr;
                      switch (result.error()) {
                          case ImportExport::ImportError::FILE_NOT_FOUND:
                              error_msg = "File not found";
                              break;
                          case ImportExport::ImportError::INVALID_FORMAT:
                              error_msg = "Invalid CSV format";
                              break;
                          case ImportExport::ImportError::PARSE_ERROR:
                              error_msg = "Failed to parse CSV file";
                              break;
                          case ImportExport::ImportError::UNSUPPORTED_VERSION:
                              error_msg = "Unsupported file version";
                              break;
                          case ImportExport::ImportError::EMPTY_FILE:
                              error_msg = "Empty file";
                              break;
                          case ImportExport::ImportError::ENCRYPTION_ERROR:
                              error_msg = "Encryption error";
                              break;
                      }
                      return std::unexpected(error_msg ? std::string(error_msg)
                                                       : std::string("Unknown error"));
                  }

                  if (!m_vault_manager) {
                      return std::unexpected("vault is not open");
                  }

                  auto& accounts = result.value();

                  int imported_count = 0;
                  int failed_count = 0;
                  std::vector<std::string> failed_accounts;

                  for (const auto& account : accounts) {
                      if (m_vault_manager->add_account(account)) {
                          imported_count++;
                      } else {
                          failed_count++;
                          if (failed_accounts.size() < 10) {
                              failed_accounts.push_back(account.account_name());
                          }
                      }
                  }

                  return Flows::ImportOperationPort::Summary{
                      .format_name = std::move(format_name),
                      .imported_count = imported_count,
                      .failed_count = failed_count,
                      .failed_accounts = std::move(failed_accounts),
                  };
              },
          },
      })
    , m_export_flow(Flows::ExportFlowController::Ports{
          Flows::MessagePort{
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_info_dialog(message, title);
              },
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_warning_dialog(message, title);
              },
              [this](const std::string& message, const std::string& title) {
                  m_dialog_manager->show_error_dialog(message, title);
              },
          },
          Flows::SchedulerPort{
              [](std::function<void()> fn) {
                  Glib::signal_idle().connect_once(std::move(fn));
              },
          },
          Flows::ExportWarningPort{
              [this](std::function<void(bool)> cb) {
                  auto warning_dialog = Gtk::make_managed<Gtk::MessageDialog>(
                      m_window,
                      "Export Accounts to Plaintext?",
                      false,
                      Gtk::MessageType::WARNING,
                      Gtk::ButtonsType::NONE,
                      true);
                  warning_dialog->set_modal(true);
                  warning_dialog->set_hide_on_close(true);
                  warning_dialog->set_secondary_text(
                      "Warning: ALL export formats save passwords in UNENCRYPTED PLAINTEXT.\n\n"
                      "Supported formats: CSV, KeePass XML, 1Password 1PIF\n\n"
                      "The exported file will NOT be encrypted. Anyone with access to the file\n"
                      "will be able to read all your passwords.\n\n"
                      "To proceed, you must re-authenticate with your master password.");

                  warning_dialog->add_button("_Cancel", Gtk::ResponseType::CANCEL);
                  auto export_button = warning_dialog->add_button("_Continue", Gtk::ResponseType::OK);
                  export_button->add_css_class("destructive-action");

                  warning_dialog->signal_response().connect(
                      [this, warning_dialog, cb = std::move(cb)](int response) mutable {
                          try {
                              warning_dialog->hide();
                              cb(response == Gtk::ResponseType::OK);
                          } catch (const std::exception& e) {
                              KeepTower::Log::error(
                                  "Exception in export warning handler: {}", e.what());
                              m_dialog_manager->show_error_dialog(
                                  std::format("Export failed: {}", e.what()));
                          } catch (...) {
                              KeepTower::Log::error("Unknown exception in export warning handler");
                              m_dialog_manager->show_error_dialog(
                                  "Export failed due to unknown error");
                          }
                      });

                  warning_dialog->show();
              },
          },
          Flows::PasswordPromptPort{
              [this]() -> std::optional<std::string> {
                  if (!m_vault_manager) {
                      return std::nullopt;
                  }
                  if (!m_vault_manager->is_v2_vault()) {
                      return std::nullopt;
                  }
                  auto current_username = m_vault_manager->get_current_username();
                  if (current_username.empty()) {
                      return std::nullopt;
                  }
                  return current_username;
              },
              [this](std::function<void(std::string)> cb) {
                  auto* password_dialog = Gtk::make_managed<PasswordDialog>(m_window);

                  std::string current_username;
                  bool is_v2_vault = false;
                  if (m_vault_manager) {
                      is_v2_vault = m_vault_manager->is_v2_vault();
                      if (is_v2_vault) {
                          current_username = m_vault_manager->get_current_username();
                          if (!current_username.empty()) {
                              password_dialog->set_title(KeepTower::make_valid_utf8(
                                  std::format("Authenticate to Export (User: {})", current_username),
                                  "export_dialog_title"));
                          } else {
                              password_dialog->set_title("Authenticate to Export");
                          }
                      } else {
                          password_dialog->set_title("Authenticate to Export");
                      }
                  } else {
                      password_dialog->set_title("Authenticate to Export");
                  }

                  password_dialog->set_modal(true);
                  password_dialog->set_hide_on_close(true);

                  password_dialog->signal_response().connect(
                      [password_dialog, cb = std::move(cb)](int response) mutable {
                          if (response != Gtk::ResponseType::OK) {
                              password_dialog->hide();
                              return;
                          }

                          try {
                              std::string password = password_dialog->get_password().raw();
                              password_dialog->hide();
                              cb(std::move(password));
                          } catch (...) {
                              password_dialog->hide();
                              throw;
                          }
                      });

                  password_dialog->show();
              },
          },
          Flows::SecretCleanerPort{
              [](std::string& secret) {
                  if (!secret.empty()) {
                      OPENSSL_cleanse(secret.data(), secret.size());
                      secret.clear();
                  }
              },
          },
          Flows::ExportAuthPort{
              [this](const std::string& password_utf8) -> std::expected<void, std::string> {
                  if (!m_vault_manager) {
                      return std::unexpected("Export cancelled: vault is not open");
                  }

                  Glib::ustring password = password_utf8;

#ifdef HAVE_YUBIKEY_SUPPORT
                  bool yubikey_required = m_vault_manager->current_user_requires_yubikey();

                  if (yubikey_required) {
                      YubiKeyManager yk_manager;
                      if (!yk_manager.initialize() || !yk_manager.is_yubikey_present()) {
                          if (!password.empty()) {
                              OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                              password.clear();
                          }
                          return std::unexpected("YubiKey not detected.");
                      }

                      auto device_info = yk_manager.get_device_info();
                      if (!device_info) {
                          if (!password.empty()) {
                              OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                              password.clear();
                          }
                          return std::unexpected("Failed to get YubiKey information.");
                      }

                      std::string serial_number = device_info->serial_number;

                      auto* touch_dialog = Gtk::make_managed<YubiKeyPromptDialog>(
                          m_window,
                          YubiKeyPromptDialog::PromptType::TOUCH);
                      touch_dialog->present();

                      auto context = Glib::MainContext::get_default();
                      while (context && context->pending()) {
                          context->iteration(false);
                      }

                      g_usleep(150000);

                      bool auth_success = m_vault_manager->verify_credentials(password, serial_number);

                      touch_dialog->hide();

                      if (!password.empty()) {
                          OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                          password.clear();
                      }

                      if (!auth_success) {
                          return std::unexpected("YubiKey authentication failed. Export cancelled.");
                      }

                      return {};
                  }
#endif

                  bool auth_success = m_vault_manager->verify_credentials(password);

                  if (!password.empty()) {
                      OPENSSL_cleanse(const_cast<char*>(password.data()), password.bytes());
                      password.clear();
                  }

                  if (!auth_success) {
                      return std::unexpected("Authentication failed. Export cancelled.");
                  }

                  return {};
              },
          },
          Flows::FileDialogPort{
              {},
              [this](const std::string& title,
                     const std::string& suggested_name,
                     const Flows::FileDialogPort::Filters& filters,
                     std::function<void(const std::string&)> cb) {
                  m_dialog_manager->show_save_file_dialog(
                      title,
                      suggested_name,
                      [cb = std::move(cb)](std::string path) {
                          cb(path);
                      },
                      filters);
              },
          },
          Flows::ExportOperationPort{
              [this](const std::string& dest_path)
                  -> std::expected<Flows::ExportOperationPort::Success, std::string> {
                  if (!m_vault_manager) {
                      return std::unexpected("Export cancelled: vault is not open");
                  }

                  if (dest_path.empty()) {
                      return std::unexpected("No file was selected");
                  }

                  std::vector<keeptower::AccountRecord> accounts;
                  int account_count = m_vault_manager->get_account_count();
                  accounts.reserve(account_count);

                  for (int i = 0; i < account_count; i++) {
                      const auto* account = m_vault_manager->get_account(i);
                      if (account) {
                          accounts.emplace_back(*account);
                      }
                  }

                  std::expected<void, ImportExport::ExportError> result;
                  std::string format_name;
                  std::string warning_text = "Warning: This file contains UNENCRYPTED passwords!";

                  if (dest_path.ends_with(".xml")) {
                      result = ImportExport::export_to_keepass_xml(dest_path, accounts);
                      format_name = "KeePass XML";
                      warning_text += "\n\nNOTE: KeePass import compatibility not fully tested.";
                  } else if (dest_path.ends_with(".1pif")) {
                      result = ImportExport::export_to_1password_1pif(dest_path, accounts);
                      format_name = "1Password 1PIF";
                      warning_text += "\n\nNOTE: 1Password import compatibility not fully tested.";
                  } else {
                      result = ImportExport::export_to_csv(dest_path, accounts);
                      format_name = "CSV";
                  }

                  if (!result.has_value()) {
                      const char* error_msg = nullptr;
                      switch (result.error()) {
                          case ImportExport::ExportError::FILE_WRITE_ERROR:
                              error_msg = "Failed to write file";
                              break;
                          case ImportExport::ExportError::PERMISSION_DENIED:
                              error_msg = "Permission denied";
                              break;
                          case ImportExport::ExportError::INVALID_DATA:
                              error_msg = "Invalid data";
                              break;
                      }
                      return std::unexpected(error_msg ? std::string(error_msg)
                                                       : std::string("Unknown error"));
                  }

                  return Flows::ExportOperationPort::Success{
                      .path = dest_path,
                      .format_name = std::move(format_name),
                      .warning_text = std::move(warning_text),
                      .account_count = accounts.size(),
                  };
              },
          },
    }) {
}

void VaultIOHandler::handle_import(const UpdateCallback& on_update) {
    m_import_flow.start_import(on_update);
}

void VaultIOHandler::handle_export(const std::string& current_vault_path, bool vault_open) {
    m_export_flow.start_export(current_vault_path, vault_open);
}

} // namespace UI
