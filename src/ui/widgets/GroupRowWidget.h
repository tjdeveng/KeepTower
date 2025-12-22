

#pragma once
#include <sigc++/sigc++.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/revealer.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <string>

// Forward declaration
namespace keeptower {
    class AccountGroup;
}

class GroupRowWidget : public Gtk::Box {
public:
    GroupRowWidget();
    ~GroupRowWidget() override;

    // Set group data (from keeptower::AccountGroup)
    void set_group(const keeptower::AccountGroup& group);

    // Get current group id
    std::string group_id() const;

    // Set selected state
    void set_selected(bool selected);

    // Expand/collapse the group
    void set_expanded(bool expanded);
    bool is_expanded() const;

    // Add a child widget (account row) to this group
    void add_child(Gtk::Widget& child);

    // Clear all child widgets
    void clear_children();

    // Signal: emitted when this group is selected
    sigc::signal<void(std::string)>& signal_selected();

    // Signal: emitted when this group is reordered (drag-and-drop)
    sigc::signal<void(std::string, int)>& signal_reordered();

    // Signal: emitted when an account is dropped into this group
    sigc::signal<void(std::string, std::string)>& signal_account_dropped();  // account_id, group_id

    // Signal: emitted when this group is right-clicked
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_right_clicked();

private:
    // Header row (clickable)
    Gtk::Box m_header_box;
    Gtk::Image m_disclosure_icon;  // ▶/▼ triangle
    Gtk::Image m_icon;
    Gtk::Label m_label;

    // Children container (expandable/collapsible)
    Gtk::Revealer m_revealer;
    Gtk::Box m_children_box;

    // Group data
    std::string m_group_id;
    bool m_expanded = true;  // Groups start expanded by default

    // Gesture for click handling
    Glib::RefPtr<Gtk::GestureClick> m_click_gesture;
    Glib::RefPtr<Gtk::GestureClick> m_right_click_gesture;

    // Drag and drop support
    Glib::RefPtr<Gtk::DragSource> m_drag_source;
    Glib::RefPtr<Gtk::DropTarget> m_drop_target;

    // Signals
    sigc::signal<void(std::string)> m_signal_selected;
    sigc::signal<void(std::string, int)> m_signal_reordered;
    sigc::signal<void(std::string, std::string)> m_signal_account_dropped;
    sigc::signal<void(std::string, Gtk::Widget*, double, double)> m_signal_right_clicked;

    // Helpers
    void update_display();
    void on_header_clicked(int n_press, double x, double y);
    void on_header_right_clicked(int n_press, double x, double y);

    // Drag and drop handlers
    void setup_drag_and_drop();
    Glib::RefPtr<Gdk::ContentProvider> on_drag_prepare(double x, double y);
    void on_drag_begin(const Glib::RefPtr<Gdk::Drag>& drag);
    bool on_drop(const Glib::ValueBase& value, double x, double y);
    bool m_selected = false;
};
