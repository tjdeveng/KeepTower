// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file UndoManager.h
 * @brief Manages undo/redo history for vault operations
 */

#ifndef UNDO_MANAGER_H
#define UNDO_MANAGER_H

#include "Command.h"
#include <vector>
#include <memory>
#include <functional>

/**
 * @brief Manages command history and undo/redo operations
 *
 * Maintains two stacks: undo history and redo history. When a new command
 * is executed, it's added to the undo stack and the redo stack is cleared.
 *
 * @section limits History Limits
 * Maintains a configurable maximum history size (default 50 commands) to
 * prevent unbounded memory growth. Oldest commands are discarded when the
 * limit is reached.
 *
 * @section thread_safety Thread Safety
 * NOT thread-safe. All operations must be called from the UI thread.
 *
 * @section example Usage Example
 * @code
 * UndoManager undo_manager;
 *
 * auto cmd = std::make_unique<AddAccountCommand>(vault_mgr, account_data);
 * undo_manager.execute_command(std::move(cmd));
 *
 * // Later...
 * if (undo_manager.can_undo()) {
 *     undo_manager.undo();
 * }
 *
 * if (undo_manager.can_redo()) {
 *     undo_manager.redo();
 * }
 * @endcode
 */
class UndoManager {
public:
    /**
     * @brief Callback function for notifying state changes
     *
     * Called when undo/redo availability changes. Use this to update
     * UI elements (e.g., enable/disable menu items).
     */
    using StateChangedCallback = std::function<void(bool can_undo, bool can_redo)>;

    /**
     * @brief Default maximum number of commands in history
     */
    static constexpr size_t DEFAULT_MAX_HISTORY = 50;

    /**
     * @brief Construct undo manager
     * @param max_history Maximum commands to keep in history
     */
    explicit UndoManager(size_t max_history = DEFAULT_MAX_HISTORY)
        : m_max_history(max_history) {}

    /**
     * @brief Execute a command and add it to history
     * @param command Command to execute
     * @return true if command executed successfully, false on error
     *
     * Executes the command and adds it to the undo stack. Clears the
     * redo stack since we're creating a new timeline branch.
     */
    [[nodiscard]] bool execute_command(std::unique_ptr<Command> command) {
        if (!command) {
            return false;
        }

        // Execute the command
        if (!command->execute()) {
            return false;
        }

        // Clear redo stack - we're on a new timeline
        m_redo_stack.clear();

        // Try to merge with previous command if possible
        if (!m_undo_stack.empty() &&
            m_undo_stack.back()->can_merge_with(command.get())) {
            m_undo_stack.back()->merge_with(command.get());
        } else {
            // Add to undo stack
            m_undo_stack.push_back(std::move(command));

            // Enforce history limit
            if (m_undo_stack.size() > m_max_history) {
                m_undo_stack.erase(m_undo_stack.begin());
            }
        }

        notify_state_changed();
        return true;
    }

    /**
     * @brief Undo the most recent command
     * @return true if undo successful, false if nothing to undo or error
     */
    [[nodiscard]] bool undo() {
        if (m_undo_stack.empty()) {
            return false;
        }

        auto& command = m_undo_stack.back();
        if (!command->undo()) {
            return false;
        }

        // Move to redo stack
        m_redo_stack.push_back(std::move(command));
        m_undo_stack.pop_back();

        notify_state_changed();
        return true;
    }

    /**
     * @brief Redo the most recently undone command
     * @return true if redo successful, false if nothing to redo or error
     */
    [[nodiscard]] bool redo() {
        if (m_redo_stack.empty()) {
            return false;
        }

        auto& command = m_redo_stack.back();
        if (!command->redo()) {
            return false;
        }

        // Move back to undo stack
        m_undo_stack.push_back(std::move(command));
        m_redo_stack.pop_back();

        notify_state_changed();
        return true;
    }

    /**
     * @brief Check if undo is available
     * @return true if there are commands to undo
     */
    [[nodiscard]] bool can_undo() const {
        return !m_undo_stack.empty();
    }

    /**
     * @brief Check if redo is available
     * @return true if there are commands to redo
     */
    [[nodiscard]] bool can_redo() const {
        return !m_redo_stack.empty();
    }

    /**
     * @brief Get description of next undo operation
     * @return Description string, or empty if nothing to undo
     */
    [[nodiscard]] std::string get_undo_description() const {
        if (m_undo_stack.empty()) {
            return "";
        }
        return m_undo_stack.back()->get_description();
    }

    /**
     * @brief Get description of next redo operation
     * @return Description string, or empty if nothing to redo
     */
    [[nodiscard]] std::string get_redo_description() const {
        if (m_redo_stack.empty()) {
            return "";
        }
        return m_redo_stack.back()->get_description();
    }

    /**
     * @brief Clear all history
     *
     * Removes all commands from both undo and redo stacks. Call this
     * when closing a vault to prevent operations on stale data.
     */
    void clear() {
        m_undo_stack.clear();
        m_redo_stack.clear();
        notify_state_changed();
    }

    /**
     * @brief Set callback for state change notifications
     * @param callback Function to call when undo/redo availability changes
     */
    void set_state_changed_callback(StateChangedCallback callback) {
        m_state_changed_callback = std::move(callback);
        // Notify immediately to update UI
        notify_state_changed();
    }

    /**
     * @brief Get number of commands in undo history
     * @return Count of undoable commands
     */
    [[nodiscard]] size_t get_undo_count() const {
        return m_undo_stack.size();
    }

    /**
     * @brief Get number of commands in redo history
     * @return Count of redoable commands
     */
    [[nodiscard]] size_t get_redo_count() const {
        return m_redo_stack.size();
    }

    /**
     * @brief Set maximum history size
     * @param max_history New maximum (must be > 0)
     *
     * If new limit is smaller than current history, oldest commands
     * are discarded.
     */
    void set_max_history(size_t max_history) {
        if (max_history == 0) {
            max_history = 1;  // Enforce minimum
        }

        m_max_history = max_history;

        // Trim undo stack if needed
        while (m_undo_stack.size() > m_max_history) {
            m_undo_stack.erase(m_undo_stack.begin());
        }

        // Trim redo stack if needed
        while (m_redo_stack.size() > m_max_history) {
            m_redo_stack.erase(m_redo_stack.begin());
        }

        notify_state_changed();
    }

    /**
     * @brief Get maximum history size
     * @return Current maximum number of commands
     */
    [[nodiscard]] size_t get_max_history() const {
        return m_max_history;
    }

private:
    void notify_state_changed() {
        if (m_state_changed_callback) {
            m_state_changed_callback(can_undo(), can_redo());
        }
    }

    std::vector<std::unique_ptr<Command>> m_undo_stack;
    std::vector<std::unique_ptr<Command>> m_redo_stack;
    size_t m_max_history;
    StateChangedCallback m_state_changed_callback;
};

#endif // UNDO_MANAGER_H
