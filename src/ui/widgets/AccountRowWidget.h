

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

class AccountRowWidget : public Gtk::Box {
public:
    AccountRowWidget();
    ~AccountRowWidget() override;

    // Set account data (from keeptower::AccountRecord)
    void set_account(const keeptower::AccountRecord& account);

    // Get current account id
    std::string account_id() const;

    // Set selected state
    void set_selected(bool selected);

    // Signal: emitted when this account is selected
    sigc::signal<void(std::string)>& signal_selected();

    // Signal: emitted when favorite star is toggled
    sigc::signal<void(std::string)>& signal_favorite_toggled();

    // Signal: emitted when this account is reordered (drag-and-drop)
    sigc::signal<void(std::string, int)>& signal_reordered();

    // Signal: emitted when an account is dropped onto this account
    sigc::signal<void(std::string, std::string)>& signal_account_dropped_on_account();  // dragged_id, target_id

    // Signal: emitted when this account is right-clicked
    sigc::signal<void(std::string, Gtk::Widget*, double, double)>& signal_right_clicked();

private:
    // Internal widgets
    Gtk::Image m_favorite_icon;
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
