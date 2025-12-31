// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file AccountRowWidget.h
 * @brief Custom GTK4 widget for displaying account entries in list view
 *
 * Provides a rich, interactive row widget for password accounts with:
 * - Favorite star toggle
 * - Drag-and-drop reordering
 * - Right-click context menu support
 * - Selection highlighting
 */

#pragma once
#include <sigc++/sigc++.h>
#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
#include <gtkmm/enums.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/dragsource.h>
#include <gtkmm/droptarget.h>
#include <string>

// Forward declaration
namespace keeptower {
    class AccountRecord;
}

/**
 * @class AccountRowWidget
 * @brief Interactive account list row with drag-and-drop support
 *
 * Custom GTK4 widget that displays a single account entry with:
 * - Account name label
 * - Favorite star icon (toggleable)
 * - Visual selection state
 * - Drag-and-drop reordering
 * - Right-click context menu integration
 */
class AccountRowWidget : public Gtk::Box {
public:
    /** @brief Construct empty account row widget */
    AccountRowWidget();

    /** @brief Destructor */
    ~AccountRowWidget() override;

    /**
     * @brief Set account data to display
     * @param account AccountRecord protobuf message
     */
    void set_account(const keeptower::AccountRecord& account);

    /**
     * @brief Get current account ID
     * @return Account unique identifier
     */
    std::string account_id() const;

    /**
     * @brief Set visual selection state
     * @param selected true to highlight as selected
     */
    void set_selected(bool selected);

    /**
     * @brief Signal emitted when account is clicked
     * @return Signal with account_id parameter
     */
    sigc::signal<void(std::string)>& signal_selected();

    /**
     * @brief Signal emitted when favorite star is toggled
     * @return Signal with account_id parameter
     */
    sigc::signal<void(std::string)>& signal_favorite_toggled();

    /**
     * @brief Signal emitted during drag-and-drop reorder
     * @return Signal with (account_id, new_position) parameters
     */
    sigc::signal<void(std::string, int)>& signal_reordered();

    /**
     * @brief Signal emitted when another account is dropped on this one
     * @return Signal with (dragged_id, target_id) parameters
     */
    sigc::signal<void(std::string, std::string)>& signal_account_dropped_on_account();

    /**
     * @brief Signal emitted on right-click for context menu
     * @return Signal with (account_id, widget, x, y) parameters
     */
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_right_clicked();

private:
    Gtk::Image m_favorite_icon;  ///< Favorite star icon
    Gtk::Label m_label;
    Gtk::Label m_username_label;

    // Account data
    std::string m_account_id;
    bool m_is_favorite = false;

    // Signals
    sigc::signal<void(std::string)> m_signal_selected;
    sigc::signal<void(std::string)> m_signal_favorite_toggled;
    sigc::signal<void(std::string, int)> m_signal_reordered;
    sigc::signal<void(std::string, std::string)> m_signal_account_dropped_on_account;
    sigc::signal<void(std::string, Gtk::Widget*, double, double)> m_signal_right_clicked;

    // Gesture and drag controllers
    Glib::RefPtr<Gtk::GestureClick> m_click_gesture;
    Glib::RefPtr<Gtk::GestureClick> m_star_click_gesture;
    Glib::RefPtr<Gtk::GestureClick> m_right_click_gesture;
    Glib::RefPtr<Gtk::DragSource> m_drag_source;
    Glib::RefPtr<Gtk::DropTarget> m_drop_target;

    // Helpers
    void update_display();
    void setup_interactions();
    void on_clicked(int n_press, double x, double y);
    void on_star_clicked(int n_press, double x, double y);
    void on_right_clicked(int n_press, double x, double y);

    // Drag and drop handlers
    Glib::RefPtr<Gdk::ContentProvider> on_drag_prepare(double x, double y);
    void on_drag_begin(const Glib::RefPtr<Gdk::Drag>& drag);
    bool on_drop(const Glib::ValueBase& value, double x, double y);
    bool m_selected = false;
};
