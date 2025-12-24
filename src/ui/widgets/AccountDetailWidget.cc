#include "AccountDetailWidget.h"
#include "src/record.pb.h"
#include "../../utils/StringHelpers.h"
#include <gtkmm.h>

using KeepTower::safe_ustring_to_string;

AccountDetailWidget::AccountDetailWidget()
    : Gtk::ScrolledWindow()
    , m_details_box(Gtk::Orientation::VERTICAL, 0)
    , m_details_fields_box(Gtk::Orientation::VERTICAL, 0)
    , m_password_visible(false)
    , m_is_modified(false)
{
    // Configure this scrolled window
    set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    set_child(m_details_box);

    // Setup main details box
    m_details_box.set_margin_start(18);
    m_details_box.set_margin_end(18);
    m_details_box.set_margin_top(18);
    m_details_box.set_margin_bottom(18);

    // Left side: Input fields
    m_account_name_label.set_text("Account Name:");
    m_account_name_label.set_xalign(0.0);
    m_account_name_entry.set_margin_bottom(12);

    m_user_name_label.set_text("Username:");
    m_user_name_label.set_xalign(0.0);
    m_user_name_entry.set_margin_bottom(12);

    m_password_label.set_text("Password:");
    m_password_label.set_xalign(0.0);
    auto* password_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 6);
    m_password_entry.set_hexpand(true);
    m_password_entry.set_visibility(false);
    password_box->append(m_password_entry);
    password_box->append(m_generate_password_button);
    password_box->append(m_show_password_button);
    password_box->append(m_copy_password_button);

    m_email_label.set_text("Email:");
    m_email_label.set_xalign(0.0);
    m_email_entry.set_margin_bottom(12);

    m_website_label.set_text("Website:");
    m_website_label.set_xalign(0.0);
    m_website_entry.set_margin_bottom(12);

    // Tags configuration
    m_tags_label.set_text("Tags:");
    m_tags_label.set_xalign(0.0);
    m_tags_entry.set_placeholder_text("Add tag (press Enter)");
    m_tags_entry.set_margin_bottom(6);
    m_tags_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::NEVER);
    m_tags_scrolled.set_min_content_height(40);
    m_tags_scrolled.set_max_content_height(120);
    m_tags_scrolled.set_child(m_tags_flowbox);
    m_tags_scrolled.set_margin_bottom(12);
    m_tags_flowbox.set_selection_mode(Gtk::SelectionMode::NONE);
    m_tags_flowbox.set_max_children_per_line(10);
    m_tags_flowbox.set_homogeneous(false);

    // Build left side fields box
    m_details_fields_box.append(m_account_name_label);
    m_details_fields_box.append(m_account_name_entry);
    m_details_fields_box.append(m_user_name_label);
    m_details_fields_box.append(m_user_name_entry);
    m_details_fields_box.append(m_password_label);
    m_details_fields_box.append(*password_box);
    m_details_fields_box.append(m_email_label);
    m_details_fields_box.append(m_email_entry);
    m_details_fields_box.append(m_website_label);
    m_details_fields_box.append(m_website_entry);
    m_details_fields_box.append(m_tags_label);
    m_details_fields_box.append(m_tags_entry);
    m_details_fields_box.append(m_tags_scrolled);

    // Privacy controls (V2 multi-user vaults)
    m_privacy_label.set_markup("<b>Privacy Controls</b> (Multi-User Vaults)");
    m_privacy_label.set_xalign(0.0);
    m_privacy_label.set_margin_top(12);
    m_privacy_label.set_margin_bottom(6);
    m_admin_only_viewable_check.set_label("Admin-only viewable");
    m_admin_only_viewable_check.set_tooltip_text(
        "Only administrators can view/edit this account. "
        "Standard users will not see this account in the list."
    );
    m_admin_only_deletable_check.set_label("Admin-only deletable");
    m_admin_only_deletable_check.set_tooltip_text(
        "All users can view/edit, but only admins can delete. "
        "Prevents accidental deletion of critical accounts."
    );
    m_details_fields_box.append(m_privacy_label);
    m_details_fields_box.append(m_admin_only_viewable_check);
    m_details_fields_box.append(m_admin_only_deletable_check);

    // Right side: Notes (with label above)
    auto* notes_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
    m_notes_label.set_text("Notes:");
    m_notes_label.set_xalign(0.0);
    m_notes_label.set_margin_bottom(6);
    m_notes_scrolled.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    m_notes_scrolled.set_vexpand(true);
    m_notes_scrolled.set_hexpand(true);
    m_notes_scrolled.set_child(m_notes_view);
    notes_box->append(m_notes_label);
    notes_box->append(m_notes_scrolled);

    // Configure horizontal resizable split: fields on left, notes on right
    m_details_paned.set_wide_handle(true);  // Make drag handle more visible
    m_details_paned.set_position(400);  // Initial split position (adjust as needed)
    m_details_paned.set_resize_start_child(false);  // Fields don't resize with window
    m_details_paned.set_resize_end_child(true);  // Notes resize with window
    m_details_paned.set_shrink_start_child(false);  // Don't shrink fields below min
    m_details_paned.set_shrink_end_child(false);  // Don't shrink notes below min
    m_details_paned.set_start_child(m_details_fields_box);
    m_details_paned.set_end_child(*notes_box);

    // Main details box: resizable split + delete button at bottom
    m_details_box.append(m_details_paned);

    // Delete button at bottom (HIG compliant placement)
    m_delete_account_button.set_label("Delete Account");
    m_delete_account_button.set_icon_name("user-trash-symbolic");
    m_delete_account_button.add_css_class("destructive-action");
    m_delete_account_button.set_sensitive(false);
    m_delete_account_button.set_margin_top(12);
    m_details_box.append(m_delete_account_button);

    // Set remaining button icons
    m_generate_password_button.set_icon_name("view-refresh-symbolic");
    m_generate_password_button.set_tooltip_text("Generate Password");
    m_show_password_button.set_icon_name("view-reveal-symbolic");
    m_show_password_button.set_tooltip_text("Show/Hide Password");
    m_copy_password_button.set_icon_name("edit-copy-symbolic");
    m_copy_password_button.set_tooltip_text("Copy Password");

    // Connect signals
    m_show_password_button.signal_clicked().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_show_password_clicked)
    );
    m_generate_password_button.signal_clicked().connect([this]() {
        m_signal_generate_password.emit();
    });
    m_copy_password_button.signal_clicked().connect([this]() {
        m_signal_copy_password.emit();
    });
    m_delete_account_button.signal_clicked().connect([this]() {
        m_signal_delete_requested.emit();
    });

    // Connect change signals for all entry fields
    m_account_name_entry.signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_user_name_entry.signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_password_entry.signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_email_entry.signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_website_entry.signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_admin_only_viewable_check.signal_toggled().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_admin_only_deletable_check.signal_toggled().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );
    m_notes_view.get_buffer()->signal_changed().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_entry_changed)
    );

    // Connect tag entry
    m_tags_entry.signal_activate().connect(
        sigc::mem_fun(*this, &AccountDetailWidget::on_tag_entry_activate)
    );

    // Initially clear/disable
    clear();
}

AccountDetailWidget::~AccountDetailWidget() {
    // Securely clear password field before destruction
    // GTK4 Entry doesn't expose underlying buffer, so we overwrite multiple times
    // This is a best-effort approach given GTK4's API limitations
    secure_clear_password();
}

void AccountDetailWidget::display_account(const keeptower::AccountRecord* account) {
    if (!account) {
        clear();
        return;
    }

    // Populate fields - convert std::string to Glib::ustring
    m_account_name_entry.set_text(Glib::ustring(account->account_name()));
    m_user_name_entry.set_text(Glib::ustring(account->user_name()));
    m_password_entry.set_text(Glib::ustring(account->password()));
    m_email_entry.set_text(Glib::ustring(account->email()));
    m_website_entry.set_text(Glib::ustring(account->website()));
    m_notes_view.get_buffer()->set_text(Glib::ustring(account->notes()));

    // Clear and populate tags
    while (auto child = m_tags_flowbox.get_first_child()) {
        m_tags_flowbox.remove(*child);
    }
    for (const auto& tag : account->tags()) {
        add_tag_chip(tag);
    }

    // Set privacy controls (V2 multi-user vaults)
    m_admin_only_viewable_check.set_active(account->is_admin_only_viewable());
    m_admin_only_deletable_check.set_active(account->is_admin_only_deletable());

    // Enable widgets
    set_editable(true);
    m_delete_account_button.set_sensitive(true);

    // Reset modified flag when loading account
    m_is_modified = false;
}

void AccountDetailWidget::clear() {
    // Securely clear password before setting new text
    secure_clear_password();

    m_account_name_entry.set_text("");
    m_user_name_entry.set_text("");
    m_password_entry.set_text("");
    m_email_entry.set_text("");
    m_website_entry.set_text("");
    m_notes_view.get_buffer()->set_text("");
    m_tags_entry.set_text("");

    // Clear tags
    while (auto child = m_tags_flowbox.get_first_child()) {
        m_tags_flowbox.remove(*child);
    }

    // Clear privacy controls
    m_admin_only_viewable_check.set_active(false);
    m_admin_only_deletable_check.set_active(false);

    set_editable(false);
    m_delete_account_button.set_sensitive(false);

    // Reset modified flag
    m_is_modified = false;
}

std::string AccountDetailWidget::get_account_name() const {
    return safe_ustring_to_string(m_account_name_entry.get_text(), "account_name");
}

std::string AccountDetailWidget::get_user_name() const {
    return safe_ustring_to_string(m_user_name_entry.get_text(), "user_name");
}

std::string AccountDetailWidget::get_password() const {
    return safe_ustring_to_string(m_password_entry.get_text(), "password");
}

std::string AccountDetailWidget::get_email() const {
    return safe_ustring_to_string(m_email_entry.get_text(), "email");
}

std::string AccountDetailWidget::get_website() const {
    return safe_ustring_to_string(m_website_entry.get_text(), "website");
}

std::string AccountDetailWidget::get_notes() const {
    return safe_ustring_to_string(m_notes_view.get_buffer()->get_text(), "notes");
}

std::string AccountDetailWidget::get_tags() const {
    return safe_ustring_to_string(m_tags_entry.get_text(), "tags");
}

std::vector<std::string> AccountDetailWidget::get_all_tags() const {
    std::vector<std::string> tags;

    // Iterate through all tag chips in the flowbox
    for (auto child = m_tags_flowbox.get_first_child(); child; child = child->get_next_sibling()) {
        if (auto* flow_child = dynamic_cast<const Gtk::FlowBoxChild*>(child)) {
            if (auto* box = dynamic_cast<const Gtk::Box*>(flow_child->get_child())) {
                if (auto* label = dynamic_cast<const Gtk::Label*>(box->get_first_child())) {
                    tags.push_back(safe_ustring_to_string(label->get_text(), "tag"));
                }
            }
        }
    }

    return tags;
}

bool AccountDetailWidget::get_admin_only_viewable() const {
    return m_admin_only_viewable_check.get_active();
}

bool AccountDetailWidget::get_admin_only_deletable() const {
    return m_admin_only_deletable_check.get_active();
}

void AccountDetailWidget::set_editable(bool editable) {
    // Make Entry widgets read-only (they inherit from Gtk::Editable interface)
    // Using the Editable interface methods
    auto* account_editable = dynamic_cast<Gtk::Editable*>(&m_account_name_entry);
    auto* user_editable = dynamic_cast<Gtk::Editable*>(&m_user_name_entry);
    auto* password_editable = dynamic_cast<Gtk::Editable*>(&m_password_entry);
    auto* email_editable = dynamic_cast<Gtk::Editable*>(&m_email_entry);
    auto* website_editable = dynamic_cast<Gtk::Editable*>(&m_website_entry);
    auto* tags_editable = dynamic_cast<Gtk::Editable*>(&m_tags_entry);

    if (account_editable) account_editable->set_editable(editable);
    if (user_editable) user_editable->set_editable(editable);
    if (password_editable) password_editable->set_editable(editable);  // Prevents password modification/deletion
    if (email_editable) email_editable->set_editable(editable);
    if (website_editable) website_editable->set_editable(editable);
    if (tags_editable) tags_editable->set_editable(editable);

    m_notes_view.set_editable(editable);

    // Disable generate password button - admin-only function
    m_generate_password_button.set_sensitive(editable);

    // View/copy buttons remain enabled for read-only access
    m_show_password_button.set_sensitive(true);
    m_copy_password_button.set_sensitive(true);
    // Note: Privacy controls sensitivity is managed separately via set_privacy_controls_editable()
}

void AccountDetailWidget::set_privacy_controls_editable(bool editable) {
    m_admin_only_viewable_check.set_sensitive(editable);
    m_admin_only_deletable_check.set_sensitive(editable);
}

void AccountDetailWidget::set_delete_button_sensitive(bool sensitive) {
    m_delete_account_button.set_sensitive(sensitive);
}

void AccountDetailWidget::set_password(const std::string& password) {
    m_password_entry.set_text(password);
    m_signal_modified.emit();
}

void AccountDetailWidget::focus_account_name_entry() {
    m_account_name_entry.grab_focus();
    m_account_name_entry.select_region(0, -1);
}

sigc::signal<void()> AccountDetailWidget::signal_modified() {
    return m_signal_modified;
}

sigc::signal<void()> AccountDetailWidget::signal_delete_requested() {
    return m_signal_delete_requested;
}

sigc::signal<void()> AccountDetailWidget::signal_generate_password() {
    return m_signal_generate_password;
}

sigc::signal<void()> AccountDetailWidget::signal_copy_password() {
    return m_signal_copy_password;
}

void AccountDetailWidget::on_show_password_clicked() {
    m_password_visible = !m_password_visible;
    m_password_entry.set_visibility(m_password_visible);

    if (m_password_visible) {
        m_show_password_button.set_icon_name("view-conceal-symbolic");
    } else {
        m_show_password_button.set_icon_name("view-reveal-symbolic");
    }
}

void AccountDetailWidget::on_entry_changed() {
    m_is_modified = true;
    m_signal_modified.emit();
}

void AccountDetailWidget::on_tag_entry_activate() {
    std::string tag = m_tags_entry.get_text();
    if (!tag.empty()) {
        add_tag_chip(tag);
        m_tags_entry.set_text("");
        m_signal_modified.emit();
    }
}

void AccountDetailWidget::add_tag_chip(const std::string& tag) {
    auto* chip_box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    chip_box->add_css_class("tag-chip");
    chip_box->set_margin_start(4);
    chip_box->set_margin_end(4);
    chip_box->set_margin_top(4);
    chip_box->set_margin_bottom(4);

    auto* label = Gtk::make_managed<Gtk::Label>(tag);
    chip_box->append(*label);

    auto* remove_button = Gtk::make_managed<Gtk::Button>();
    remove_button->set_icon_name("window-close-symbolic");
    remove_button->add_css_class("flat");
    remove_button->signal_clicked().connect([this, chip_box, tag]() {
        remove_tag_chip(tag);
        m_signal_modified.emit();
    });
    chip_box->append(*remove_button);

    m_tags_flowbox.append(*chip_box);
}

void AccountDetailWidget::remove_tag_chip(const std::string& tag) {
    // Find and remove the chip with this tag
    for (auto child = m_tags_flowbox.get_first_child(); child; child = child->get_next_sibling()) {
        if (auto* flow_child = dynamic_cast<Gtk::FlowBoxChild*>(child)) {
            if (auto* box = dynamic_cast<Gtk::Box*>(flow_child->get_child())) {
                if (auto* label = dynamic_cast<Gtk::Label*>(box->get_first_child())) {
                    if (label->get_text().raw() == tag) {
                        m_tags_flowbox.remove(*flow_child);
                        break;
                    }
                }
            }
        }
    }
}

void AccountDetailWidget::secure_clear_password() {
    // GTK4 Entry doesn't expose the underlying char* buffer
    // Best we can do is overwrite the text multiple times before clearing
    // This is a limitation of GTK4's API - the actual buffer may still contain traces

    auto current_text = m_password_entry.get_text();
    if (!current_text.empty()) {
        const size_t len = current_text.length();

        // Overwrite with zeros
        m_password_entry.set_text(std::string(len, '\0'));

        // Overwrite with 0xFF
        m_password_entry.set_text(std::string(len, '\xFF'));

        // Overwrite with random-ish pattern
        m_password_entry.set_text(std::string(len, '\xAA'));

        // Final clear
        m_password_entry.set_text("");
    }

    // Note: This is best-effort. GTK4's Entry widget maintains internal buffers
    // we cannot access. For maximum security, passwords should never be displayed
    // in Entry widgets, only in secure custom widgets with direct buffer control.
}
