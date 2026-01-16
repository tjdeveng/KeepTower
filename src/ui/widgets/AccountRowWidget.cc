#include "AccountRowWidget.h"
#include "record.pb.h"
#include <gtkmm/icontheme.h>

AccountRowWidget::AccountRowWidget()
    : Gtk::Box(Gtk::Orientation::HORIZONTAL, 6)
{
    set_margin_top(2);
    set_margin_bottom(2);
    set_margin_start(16);
    set_margin_end(8);
    set_hexpand(true);
    set_vexpand(false);
    set_visible(true);  // Ensure visibility

    m_favorite_icon.set_pixel_size(18);
    m_favorite_icon.set_visible(true);

    m_label.set_xalign(0.0);
    m_label.set_hexpand(true);
    m_label.set_visible(true);

    append(m_favorite_icon);
    append(m_label);

    // Setup click and drag-and-drop
    setup_interactions();
}

AccountRowWidget::~AccountRowWidget() = default;

void AccountRowWidget::set_account(const keeptower::AccountRecord& account) {
    m_account_id = account.id();
    m_label.set_text(account.account_name());
    m_is_favorite = account.is_favorite();
    if (m_is_favorite) {
        m_favorite_icon.set_from_icon_name("starred-symbolic");
    } else {
        m_favorite_icon.set_from_icon_name("non-starred-symbolic");
    }
    update_display();
}

std::string AccountRowWidget::account_id() const {
    return m_account_id;
}

void AccountRowWidget::set_selected(bool selected) {
    m_selected = selected;
    update_display();
}

sigc::signal<void(std::string)>& AccountRowWidget::signal_selected() {
    return m_signal_selected;
}

sigc::signal<void(std::string)>& AccountRowWidget::signal_favorite_toggled() {
    return m_signal_favorite_toggled;
}

sigc::signal<void(std::string, int)>& AccountRowWidget::signal_reordered() {
    return m_signal_reordered;
}

sigc::signal<void(std::string, std::string)>& AccountRowWidget::signal_account_dropped_on_account() {
    return m_signal_account_dropped_on_account;
}

sigc::signal<void(std::string, Gtk::Widget*, double, double)>& AccountRowWidget::signal_right_clicked() {
    return m_signal_right_clicked;
}

void AccountRowWidget::update_display() {
    if (m_selected) {
        add_css_class("selected");
    } else {
        remove_css_class("selected");
    }
}

void AccountRowWidget::setup_interactions() {
    // Setup click gesture for star icon
    m_star_click_gesture = Gtk::GestureClick::create();
    m_star_click_gesture->signal_pressed().connect(
        sigc::mem_fun(*this, &AccountRowWidget::on_star_clicked));
    m_favorite_icon.add_controller(m_star_click_gesture);

    // Setup drag source on the label - MUST be added BEFORE click gesture
    m_drag_source = Gtk::DragSource::create();
    m_drag_source->set_actions(Gdk::DragAction::MOVE);

    m_drag_source->signal_prepare().connect(
        sigc::mem_fun(*this, &AccountRowWidget::on_drag_prepare), false);
    m_drag_source->signal_drag_begin().connect(
        sigc::mem_fun(*this, &AccountRowWidget::on_drag_begin), false);

    m_label.add_controller(m_drag_source);  // Add drag to label first

    // Setup click gesture for selection on the SAME widget (label)
    // This matches GroupRowWidget pattern where both are on same widget
    // Use released() not pressed() so drag has time to detect movement
    m_click_gesture = Gtk::GestureClick::create();
    m_click_gesture->set_button(GDK_BUTTON_PRIMARY);
    m_click_gesture->signal_released().connect(  // RELEASED, not pressed!
        sigc::mem_fun(*this, &AccountRowWidget::on_clicked));
    m_label.add_controller(m_click_gesture);  // Add click to label too

    // Setup right-click gesture for context menu
    m_right_click_gesture = Gtk::GestureClick::create();
    m_right_click_gesture->set_button(GDK_BUTTON_SECONDARY);
    m_right_click_gesture->signal_pressed().connect(
        sigc::mem_fun(*this, &AccountRowWidget::on_right_clicked));
    add_controller(m_right_click_gesture);

    // Setup drop target
    m_drop_target = Gtk::DropTarget::create(G_TYPE_STRING, Gdk::DragAction::MOVE);
    m_drop_target->signal_drop().connect(
        sigc::mem_fun(*this, &AccountRowWidget::on_drop), false);

    add_controller(m_drop_target);
}

void AccountRowWidget::on_clicked(int n_press, double x, double y) {
    // Emit selection signal
    m_signal_selected.emit(m_account_id);
}

void AccountRowWidget::on_star_clicked(int n_press, double x, double y) {
    // Emit signal to toggle favorite state - don't update local state
    // The backend will toggle it and UI refresh will show the correct state
    m_signal_favorite_toggled.emit(m_account_id);
}

void AccountRowWidget::on_right_clicked(int n_press, double x, double y) {
    // Emit signal to show context menu with click coordinates and widget
    m_signal_right_clicked.emit(m_account_id, this, x, y);
}

Glib::RefPtr<Gdk::ContentProvider> AccountRowWidget::on_drag_prepare(double x, double y) {
    // Store account ID for drag operation
    auto value = Glib::Value<Glib::ustring>();
    value.init(value.value_type());
    value.set("account:" + m_account_id);
    return Gdk::ContentProvider::create(value);
}

void AccountRowWidget::on_drag_begin(const Glib::RefPtr<Gdk::Drag>& drag) {
    // Visual feedback during drag
    add_css_class("dragging");
}

bool AccountRowWidget::on_drop(const Glib::ValueBase& value, [[maybe_unused]] double x, [[maybe_unused]] double y) {
    // Parse the dropped data - need to extract the string from GValue
    if (!G_VALUE_HOLDS_STRING(value.gobj())) {
        return false;
    }

    const char* str_value = g_value_get_string(value.gobj());
    if (!str_value) {
        return false;
    }

    std::string dropped_data = str_value;

    // Check if it's an account or group being dropped
    if (dropped_data.find("account:") == 0) {
        // An account is being dropped onto this account
        std::string dropped_account_id = dropped_data.substr(8);  // Skip "account:"

        // Emit signal with both account IDs so parent can handle the logic
        // Don't reject drops onto self - the parent will decide if it's a no-op
        // This allows "All Accounts" removal to work even when dropping on same account
        m_signal_account_dropped_on_account.emit(dropped_account_id, m_account_id);
        return true;
    }

    return false;
}
