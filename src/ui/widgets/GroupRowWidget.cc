#include "GroupRowWidget.h"
#include "record.pb.h"
#include <gtkmm/icontheme.h>

GroupRowWidget::GroupRowWidget()
    : Gtk::Box(Gtk::Orientation::VERTICAL, 0),
      m_header_box(Gtk::Orientation::HORIZONTAL, 6),
      m_children_box(Gtk::Orientation::VERTICAL, 0)
{
    set_hexpand(true);
    set_vexpand(false);
    set_visible(true);  // Ensure visibility

    // Setup header row
    m_header_box.set_margin_top(2);
    m_header_box.set_margin_bottom(2);
    m_header_box.set_margin_start(8);
    m_header_box.set_margin_end(8);
    m_header_box.set_hexpand(true);
    m_header_box.set_visible(true);

    // Disclosure triangle (▶/▼)
    m_disclosure_icon.set_pixel_size(16);
    m_disclosure_icon.set_from_icon_name("pan-down-symbolic");  // ▼ expanded
    m_disclosure_icon.set_visible(true);

    // Group icon
    m_icon.set_pixel_size(20);
    m_icon.set_visible(true);

    // Group label
    m_label.set_xalign(0.0);
    m_label.set_hexpand(true);
    m_label.set_visible(true);

    // Add widgets to header
    m_header_box.append(m_disclosure_icon);
    m_header_box.append(m_icon);
    m_header_box.append(m_label);

    // Setup click gesture for header
    m_click_gesture = Gtk::GestureClick::create();
    m_click_gesture->signal_pressed().connect(
        sigc::mem_fun(*this, &GroupRowWidget::on_header_clicked));
    m_header_box.add_controller(m_click_gesture);

    // Setup right-click gesture for context menu
    m_right_click_gesture = Gtk::GestureClick::create();
    m_right_click_gesture->set_button(GDK_BUTTON_SECONDARY);
    m_right_click_gesture->signal_pressed().connect(
        sigc::mem_fun(*this, &GroupRowWidget::on_header_right_clicked));
    m_header_box.add_controller(m_right_click_gesture);

    // Setup children container
    m_children_box.set_margin_start(32);  // Indent child accounts
    m_children_box.set_visible(true);
    m_revealer.set_child(m_children_box);
    m_revealer.set_reveal_child(m_expanded);
    m_revealer.set_transition_type(Gtk::RevealerTransitionType::SLIDE_DOWN);
    m_revealer.set_transition_duration(150);
    m_revealer.set_visible(true);

    // Add header and revealer to main box
    append(m_header_box);
    append(m_revealer);

    // Setup drag and drop for reordering
    setup_drag_and_drop();
}

GroupRowWidget::~GroupRowWidget() = default;

void GroupRowWidget::set_group(const keeptower::AccountGroup& group) {
    m_group_id = group.group_id();
    m_label.set_text(group.group_name());
    // Set icon if available, else fallback
    if (!group.icon().empty()) {
        m_icon.set_from_icon_name(group.icon());
    } else {
        m_icon.set_from_icon_name("folder-symbolic");
    }
    update_display();
}

std::string GroupRowWidget::group_id() const {
    return m_group_id;
}

void GroupRowWidget::set_selected(bool selected) {
    m_selected = selected;
    update_display();
}

void GroupRowWidget::set_expanded(bool expanded) {
    if (m_expanded == expanded) {
        return;
    }

    m_expanded = expanded;
    m_revealer.set_reveal_child(m_expanded);

    // Update disclosure icon
    if (m_expanded) {
        m_disclosure_icon.set_from_icon_name("pan-down-symbolic");  // ▼
    } else {
        m_disclosure_icon.set_from_icon_name("pan-end-symbolic");   // ▶
    }

    // Request layout recalculation
    queue_resize();
}

bool GroupRowWidget::is_expanded() const {
    return m_expanded;
}

void GroupRowWidget::add_child(Gtk::Widget& child) {
    m_children_box.append(child);
    // Ensure revealer shows the children
    m_revealer.set_reveal_child(m_expanded);
}

void GroupRowWidget::clear_children() {
    // Remove all children from the box
    while (auto child = m_children_box.get_first_child()) {
        m_children_box.remove(*child);
    }
}

sigc::signal<void(std::string)>& GroupRowWidget::signal_selected() {
    return m_signal_selected;
}

sigc::signal<void(std::string, int)>& GroupRowWidget::signal_reordered() {
    return m_signal_reordered;
}

sigc::signal<void(std::string, std::string)>& GroupRowWidget::signal_account_dropped() {
    return m_signal_account_dropped;
}

sigc::signal<void(std::string, Gtk::Widget*, double, double)>& GroupRowWidget::signal_right_clicked() {
    return m_signal_right_clicked;
}

void GroupRowWidget::on_header_clicked(int n_press, double x, double y) {
    // Toggle expansion on click - DON'T emit selection signal
    // Selection should only happen on double-click or via another mechanism
    g_debug("GroupRowWidget::on_header_clicked - group '%s', current expanded=%d",
            m_group_id.c_str(), m_expanded);
    g_debug("  Widget visible=%d, parent=%p", get_visible(), (void*)get_parent());

    set_expanded(!m_expanded);
    g_debug("  After toggle: expanded=%d", m_expanded);
    g_debug("  Widget still visible=%d", get_visible());

    // Force parent to recalculate layout - walk up the widget tree
    auto widget = static_cast<Gtk::Widget*>(this);
    int depth = 0;
    while (widget && depth < 5) {  // Limit to 5 levels
        g_debug("  Requesting resize for widget %s (visible=%d)",
                G_OBJECT_TYPE_NAME(widget->gobj()), widget->get_visible());
        widget->queue_resize();
        widget = widget->get_parent();
        depth++;
    }

    // DO NOT emit selection signal here - that triggers rebuild
    // m_signal_selected.emit(m_group_id);
}

void GroupRowWidget::update_display() {
    if (m_selected) {
        m_header_box.add_css_class("selected");
    } else {
        m_header_box.remove_css_class("selected");
    }
}

void GroupRowWidget::on_header_right_clicked(int n_press, double x, double y) {
    // Emit signal to show context menu with click coordinates and widget
    m_signal_right_clicked.emit(m_group_id, &m_header_box, x, y);
}

void GroupRowWidget::setup_drag_and_drop() {
    // Setup drag source on the header
    m_drag_source = Gtk::DragSource::create();
    m_drag_source->set_actions(Gdk::DragAction::MOVE);

    m_drag_source->signal_prepare().connect(
        sigc::mem_fun(*this, &GroupRowWidget::on_drag_prepare), false);
    m_drag_source->signal_drag_begin().connect(
        sigc::mem_fun(*this, &GroupRowWidget::on_drag_begin), false);

    m_header_box.add_controller(m_drag_source);

    // Setup drop target
    m_drop_target = Gtk::DropTarget::create(G_TYPE_STRING, Gdk::DragAction::MOVE);
    m_drop_target->signal_drop().connect(
        sigc::mem_fun(*this, &GroupRowWidget::on_drop), false);

    add_controller(m_drop_target);
}

Glib::RefPtr<Gdk::ContentProvider> GroupRowWidget::on_drag_prepare(double x, double y) {
    // Store group ID for drag operation
    auto value = Glib::Value<Glib::ustring>();
    value.init(value.value_type());
    value.set("group:" + m_group_id);
    return Gdk::ContentProvider::create(value);
}

void GroupRowWidget::on_drag_begin(const Glib::RefPtr<Gdk::Drag>& drag) {
    // Visual feedback during drag
    add_css_class("dragging");
}

bool GroupRowWidget::on_drop(const Glib::ValueBase& value, [[maybe_unused]] double x, [[maybe_unused]] double y) {
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
        // An account is being dropped into this group
        std::string dropped_account_id = dropped_data.substr(8);  // Skip "account:"

        // Emit signal to move account into this group
        m_signal_account_dropped.emit(dropped_account_id, m_group_id);
        return true;

    } else if (dropped_data.find("group:") == 0) {
        // A group is being dropped onto this group (reorder groups)
        std::string dropped_group_id = dropped_data.substr(6);  // Skip "group:"

        // Don't drop onto self
        if (dropped_group_id == m_group_id) {
            return false;
        }

        // Emit signal to reorder groups (position calculation handled by MainWindow/VaultManager)
        m_signal_reordered.emit(dropped_group_id, 0);
        return true;
    }

    return false;
}
