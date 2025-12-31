// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file GroupRowWidget.h
 * @brief Custom GTK4 widget for displaying account group entries
 *
 * Provides an expandable/collapsible group row widget with:
 * - Group name display
 * - Expand/collapse functionality
 * - Child account container
 * - Drag-and-drop reordering
 * - Right-click context menu support
 */

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

/**
 * @class GroupRowWidget
 * @brief Expandable group row with child account container
 *
 * Interactive group widget that can contain child account rows. Supports
 * expand/collapse animation, drag-and-drop reordering, and serves as a
 * drop target for accounts being moved between groups.
 */
class GroupRowWidget : public Gtk::Box {
public:
    /** @brief Construct empty group row widget */
    GroupRowWidget();

    /** @brief Destructor */
    ~GroupRowWidget() override;

    /**
     * @brief Set group data to display
     * @param group AccountGroup protobuf message
     */
    void set_group(const keeptower::AccountGroup& group);

    /**
     * @brief Get current group ID
     * @return Group unique identifier
     */
    std::string group_id() const;

    /**
     * @brief Set visual selection state
     * @param selected true to highlight as selected
     */
    void set_selected(bool selected);

    /**
     * @brief Expand or collapse group (show/hide children)
     * @param expanded true to expand, false to collapse
     */
    void set_expanded(bool expanded);

    /**
     * @brief Check if group is currently expanded
     * @return true if expanded (children visible)
     */
    bool is_expanded() const;

    /**
     * @brief Add account row as child
     * @param child Account widget to add
     */
    void add_child(Gtk::Widget& child);

    /** @brief Remove all child account widgets */
    void clear_children();

    /**
     * @brief Signal emitted when group is clicked
     * @return Signal with group_id parameter
     */
    sigc::signal<void(std::string)>& signal_selected();

    /**
     * @brief Signal emitted during drag-and-drop reorder
     * @return Signal with (group_id, new_position) parameters
     */
    sigc::signal<void(std::string, int)>& signal_reordered();

    /**
     * @brief Signal emitted when an account is dropped into this group
     * @return Signal with (account_id, group_id) parameters
     */
    sigc::signal<void(std::string, std::string)>& signal_account_dropped();

    /** @brief Signal emitted when group is right-clicked
     *  @return Signal with (group_id, widget, x, y) parameters for context menu */
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
