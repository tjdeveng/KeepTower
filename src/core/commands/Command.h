// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 tjdeveng

/**
 * @file Command.h
 * @brief Base command interface for undo/redo operations
 */

#ifndef COMMAND_H
#define COMMAND_H

#include <string>
#include <memory>

/**
 * @brief Abstract base class for undoable commands
 *
 * Implements the Command pattern for vault operations. Each command
 * encapsulates an operation and its inverse, enabling undo/redo functionality.
 *
 * @section usage Usage Example
 * @code
 * auto cmd = std::make_unique<AddAccountCommand>(vault_manager, account_data);
 * cmd->execute();  // Perform the operation
 * // ... later ...
 * cmd->undo();     // Reverse the operation
 * cmd->redo();     // Re-apply the operation
 * @endcode
 *
 * @section thread_safety Thread Safety
 * Commands are NOT thread-safe. The UndoManager is responsible for
 * serializing command execution.
 */
class Command {
public:
    virtual ~Command() = default;

    /**
     * @brief Execute the command
     * @return true if successful, false on error
     *
     * Performs the forward operation. Must be idempotent - calling
     * execute() multiple times should produce the same result.
     */
    [[nodiscard]] virtual bool execute() = 0;

    /**
     * @brief Undo the command
     * @return true if successful, false on error
     *
     * Reverses the operation performed by execute(). Must restore
     * the exact state before execute() was called.
     */
    [[nodiscard]] virtual bool undo() = 0;

    /**
     * @brief Redo the command
     * @return true if successful, false on error
     *
     * Re-applies the operation after undo(). Default implementation
     * calls execute(), but can be overridden for optimization.
     */
    [[nodiscard]] virtual bool redo() {
        return execute();
    }

    /**
     * @brief Get human-readable description of the command
     * @return Description suitable for UI display (e.g., "Add Account 'Gmail'")
     *
     * Used for displaying command history and action labels.
     */
    [[nodiscard]] virtual std::string get_description() const = 0;

    /**
     * @brief Check if command can be merged with another
     * @param other Command to potentially merge with
     * @return true if commands can be merged
     *
     * Enables command coalescing for rapid repeated operations
     * (e.g., typing in a text field). Default returns false.
     */
    [[nodiscard]] virtual bool can_merge_with([[maybe_unused]] const Command* other) const {
        return false;
    }

    /**
     * @brief Merge another command into this one
     * @param other Command to merge
     *
     * Only called if can_merge_with() returns true. Combines the
     * effects of both commands into one.
     */
    virtual void merge_with([[maybe_unused]] const Command* other) {
        // Default: do nothing
    }

protected:
    Command() = default;
    Command(const Command&) = delete;
    Command& operator=(const Command&) = delete;

    /** @brief Move constructor - allows command transfer */
    Command(Command&&) = default;

    /** @brief Move assignment operator - allows command reassignment */
    Command& operator=(Command&&) = default;
};

#endif // COMMAND_H
