# Drag-and-Drop Account Reordering - Implementation Plan

## Overview
Implement drag-and-drop functionality to allow users to manually reorder accounts, with a forward-looking data model that supports future Account Groups where accounts can belong to multiple groups simultaneously.

## Architectural Vision

### Multi-Group Membership Philosophy
**Key Insight**: Accounts should be able to appear in multiple groups at once:
- An account can be in "Favorites" (special built-in group) AND "Work" (user custom group)
- When starred, account automatically joins "Favorites" group
- When unstarred, account leaves "Favorites" group
- User can create custom groups (e.g., "Work", "Personal", "Banking")
- Dragging to a group adds membership (doesn't move exclusively)

This provides maximum flexibility and matches user mental models.

## User Experience

### Current Behavior
- Accounts are sorted automatically: **Favorites first**, then **alphabetically by name**
- `is_favorite` boolean flag determines star status
- No way to manually organize accounts
- Users cannot group related accounts together

### Phase 1: Drag-and-Drop Reordering (Foundation)
- Users can drag accounts to reorder them within current view
- Custom order is preserved across sessions
- Visual feedback during drag operation
- Undo/redo support for reordering
- **Data model prepared for multi-group membership**

### Phase 2: Account Groups (Future)
- "Favorites" becomes a special built-in group
- Users can create custom groups
- Accounts can belong to multiple groups
- Drag account to group header to add membership
- Groups can be collapsed/expanded
- Groups themselves can be reordered

## Technical Architecture

### 1. Data Model Changes

#### Protobuf Schema (`src/record.proto`)
**New group-aware data model:**

```protobuf
// NEW: Group definition
message AccountGroup {
    string group_id = 1;        // UUID for the group
    string group_name = 2;      // Display name (e.g., "Work", "Personal")
    int32 display_order = 3;    // Order in which groups appear
    bool is_expanded = 4;       // UI state: collapsed or expanded
    bool is_system_group = 5;   // true for "Favorites", false for user groups
}

// NEW: Group membership (many-to-many relationship)
message GroupMembership {
    string group_id = 1;        // Which group
    int32 display_order = 2;    // Order within that group
}

message AccountRecord {
    string account_name = 1;
    string user_name = 2;
    string password = 3;
    string email = 4;
    string website = 5;
    string notes = 6;
    repeated string tags = 7;
    int64 created_at = 8;
    int64 modified_at = 9;
    bool is_favorite = 10;      // KEPT for backward compatibility

    // NEW: Multi-group membership
    repeated GroupMembership groups = 11;  // Which groups this account belongs to

    // NEW: Global display order (for "All Accounts" view)
    int32 global_display_order = 12;  // -1 = automatic, >= 0 = custom order
}

// Root vault structure
message Vault {
    // ... existing fields ...
    repeated AccountRecord accounts = 1;

    // NEW: Group definitions
    repeated AccountGroup groups = 2;
}
```

**Migration Strategy:**
- Existing vaults: `global_display_order = -1`, `groups` is empty
- When user stars an account: auto-create "Favorites" system group (if not exists) + add membership
- When user first drags an account: assign `global_display_order` to all accounts
- `is_favorite` boolean kept for backward compatibility and quick checks

#### VaultManager Changes
**Phase 1 (Drag-and-Drop Foundation)**: Add basic reordering with group-aware infrastructure

```cpp
class VaultManager {
    // Phase 1: Global reordering in "All Accounts" view
    [[nodiscard]] bool reorder_account(size_t old_index, size_t new_index);

    // Reset all global_display_order to -1 (restore automatic sorting)
    void reset_global_display_order();

    // Check if any accounts have custom global ordering
    [[nodiscard]] bool has_custom_global_ordering() const;

    // Phase 2 (Future - Account Groups): Group management
    [[nodiscard]] std::string create_group(const std::string& name);  // Returns group_id
    [[nodiscard]] bool delete_group(const std::string& group_id);
    [[nodiscard]] bool add_account_to_group(size_t account_index, const std::string& group_id);
    [[nodiscard]] bool remove_account_from_group(size_t account_index, const std::string& group_id);
    [[nodiscard]] bool reorder_account_in_group(size_t account_index, const std::string& group_id, int new_order);

    // Helper: Get or create "Favorites" system group
    [[nodiscard]] std::string get_favorites_group_id();

    // Helper: Check if account belongs to group
    [[nodiscard]] bool is_account_in_group(size_t account_index, const std::string& group_id) const;
};
```

**Key Design Decision**:
- Phase 1 implements `global_display_order` for simple reordering
- Protobuf schema includes full group support, but Phase 1 doesn't use it yet
- Phase 2 activates group membership features
- This prevents having to migrate data twice
### 2. UI Implementation (GTK4)

#### Drag Source Setup
Enable dragging from TreeView rows:
```cpp
// In MainWindow constructor
auto drag_source = Gtk::DragSource::create();
drag_source->set_actions(Gdk::DragAction::MOVE);

// Prepare drag data
drag_source->signal_prepare().connect([this](double x, double y) {
    // Get the row being dragged
    Gtk::TreeModel::Path path;
    Gtk::TreeViewColumn* column;
    int cell_x, cell_y;

    if (m_account_tree_view.get_path_at_pos((int)x, (int)y, path, column, cell_x, cell_y)) {
        auto iter = m_account_list_store->get_iter(path);
        if (iter) {
            m_drag_source_index = (*iter)[m_columns.m_col_index];
            return Glib::Value<int>();  // Return drag data
        }
    }
    return Glib::Value<int>();  // Empty value = no drag
}, false);

m_account_tree_view.add_controller(drag_source);
```

#### Drop Target Setup
Enable dropping on TreeView rows:
```cpp
auto drop_target = Gtk::DropTarget::create(G_TYPE_INT, Gdk::DragAction::MOVE);

drop_target->signal_drop().connect([this](const Glib::ValueBase& value, double x, double y) {
    // Get the drop position
    Gtk::TreeModel::Path path;
    Gtk::TreeViewDropPosition pos;

    if (m_account_tree_view.get_dest_row_at_pos((int)x, (int)y, path, pos)) {
        auto iter = m_account_list_store->get_iter(path);
        if (iter) {
            int drop_target_index = (*iter)[m_columns.m_col_index];

            // Adjust position based on drop location
            if (pos == Gtk::TreeViewDropPosition::AFTER ||
                pos == Gtk::TreeViewDropPosition::INTO_OR_AFTER) {
                drop_target_index++;
            }

            // Execute reorder command
            execute_reorder_command(m_drag_source_index, drop_target_index);
            return true;
        }
    }
    return false;
}, false);

m_account_tree_view.add_controller(drop_target);
```

#### Visual Feedback
```cpp
// In drag_source prepare handler
drag_source->signal_drag_begin().connect([this](const Glib::RefPtr<Gdk::Drag>& drag) {
    // Create drag icon (ghost of the row being dragged)
    auto surface = create_drag_icon_for_account(m_drag_source_index);
    drag->set_icon(surface, 0, 0);
});

// Highlight drop target
drop_target->signal_motion().connect([this](double x, double y) {
    Gtk::TreeModel::Path path;
    Gtk::TreeViewDropPosition pos;

    if (m_account_tree_view.get_dest_row_at_pos((int)x, (int)y, path, pos)) {
        m_account_tree_view.set_drag_dest_row(path, pos);
        return Gdk::DragAction::MOVE;
    }
    return Gdk::DragAction::NONE;
});
```

### 3. Sorting Logic Updates

#### Current Sorting (in `update_account_list()`)
```cpp
// Sort: favorites first, then by account name alphabetically
std::sort(sorted_indices.begin(), sorted_indices.end(),
    [&accounts](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;  // Favorites first
        }
        return accounts[a.first].account_name() < accounts[b.first].account_name();
    });
```

#### Phase 1: Sorting with Global Display Order
```cpp
// Check if custom ordering is enabled
bool has_custom_order = m_vault_manager->has_custom_global_ordering();

if (has_custom_order) {
    // Sort by global_display_order
    std::sort(sorted_indices.begin(), sorted_indices.end(),
        [&accounts](const auto& a, const auto& b) {
            int order_a = accounts[a.first].global_display_order();
            int order_b = accounts[b.first].global_display_order();

            // If global_display_order is same or invalid, fall back to name
            if (order_a == order_b || order_a < 0 || order_b < 0) {
                return accounts[a.first].account_name() < accounts[b.first].account_name();
            }
            return order_a < order_b;
        });
} else {
    // Use automatic sorting (favorites first, then alphabetical)
    std::sort(sorted_indices.begin(), sorted_indices.end(),
        [&accounts](const auto& a, const auto& b) {
            if (a.second != b.second) {
                return a.second > b.second;  // Favorites first
            }
            return accounts[a.first].account_name() < accounts[b.first].account_name();
        });
}
```

#### Phase 2 (Future): Group-Based Sorting
When groups are active, the view will show:
```
📁 Favorites (system group)
   ⭐ Amazon Account
   ⭐ Gmail Account
📁 Work (user group)
   🏢 Slack Account
   🏢 GitHub Account
📁 Banking (user group)
   💰 Chase Bank
   💰 Wells Fargo
📁 Ungrouped Accounts
   📝 Random Site
```

Sorting logic will need to:
1. Sort groups by `display_order`
2. Within each group, sort accounts by their per-group `display_order`
3. Accounts can appear in multiple groups
4. "Ungrouped" is a special view for accounts with no group memberships

### 4. Reordering Command (Undo/Redo Support)

#### ReorderAccountCommand
```cpp
class ReorderAccountCommand : public Command {
public:
    ReorderAccountCommand(
        VaultManager* vault_manager,
        size_t old_index,
        size_t new_index,
        std::function<void()> ui_callback = nullptr)
        : m_vault_manager(vault_manager),
          m_old_index(old_index),
          m_new_index(new_index),
          m_ui_callback(std::move(ui_callback)) {}

    [[nodiscard]] bool execute() override {
        if (!m_vault_manager || !m_vault_manager->is_vault_open()) {
            return false;
        }

        bool success = m_vault_manager->reorder_account(m_old_index, m_new_index);

        if (success && m_ui_callback) {
            m_ui_callback();
        }

        return success;
    }

    [[nodiscard]] bool undo() override {
        if (!m_vault_manager) {
            return false;
        }

        // Reverse the reorder
        bool success = m_vault_manager->reorder_account(m_new_index, m_old_index);

        if (success && m_ui_callback) {
            m_ui_callback();
        }

        return success;
    }

    [[nodiscard]] std::string get_description() const override {
        return "Reorder Account";
    }

private:
    VaultManager* m_vault_manager;
    size_t m_old_index;
    size_t m_new_index;
    std::function<void()> m_ui_callback;
};
```

### 5. VaultManager::reorder_account() Implementation

```cpp
bool VaultManager::reorder_account(size_t old_index, size_t new_index) {
    if (!is_vault_open() || old_index >= m_vault.accounts_size() ||
        new_index >= m_vault.accounts_size() || old_index == new_index) {
        return false;
    }

    // Initialize display_order for all accounts if not already set
    if (!has_custom_ordering()) {
        for (int i = 0; i < m_vault.accounts_size(); i++) {
            m_vault.mutable_accounts(i)->set_display_order(i);
        }
    }

    // Get the account being moved
    auto* account_to_move = m_vault.mutable_accounts(old_index);

    // Calculate new display orders
    if (old_index < new_index) {
        // Moving down: shift accounts up
        int order = account_to_move->display_order();
        for (size_t i = old_index + 1; i <= new_index; i++) {
            int prev_order = m_vault.accounts(i).display_order();
            m_vault.mutable_accounts(i)->set_display_order(order);
            order = prev_order;
        }
        account_to_move->set_display_order(order);
    } else {
        // Moving up: shift accounts down
        int order = account_to_move->display_order();
        for (size_t i = old_index; i > new_index; i--) {
            int prev_order = m_vault.accounts(i - 1).display_order();
            m_vault.mutable_accounts(i)->set_display_order(order);
            order = prev_order;
        }
        account_to_move->set_display_order(order);
    }

    // Save vault
    return save_vault();
}

bool VaultManager::has_custom_ordering() const {
    if (!is_vault_open() || m_vault.accounts_size() == 0) {
        return false;
    }

    // Check if any account has display_order >= 0
    for (const auto& account : m_vault.accounts()) {
        if (account.display_order() >= 0) {
            return true;
        }
    }
    return false;
}

void VaultManager::reset_display_order() {
    if (!is_vault_open()) {
        return;
    }

    for (int i = 0; i < m_vault.accounts_size(); i++) {
        m_vault.mutable_accounts(i)->set_display_order(-1);
    }

    save_vault();
}
```

### 6. Preferences Integration

#### GSettings Schema Addition
```xml
<key name="enable-drag-drop-reorder" type="b">
    <default>true</default>
    <summary>Enable drag-and-drop reordering</summary>
    <description>
        Allow manually reordering accounts by dragging them.
        When disabled, accounts are sorted automatically (favorites first, then alphabetical).
    </description>
</key>
```

**Note**: Removed `keep-favorites-at-top` preference since Phase 2 will treat Favorites as a group.
Users will control this by:
- Collapsing/expanding the "Favorites" group
- Reordering groups (put Favorites at top/bottom)
- Much more flexible than a boolean toggle

#### PreferencesDialog UI
```
[ Sorting & Organization ]

☑ Enable drag-and-drop reordering
    Manually organize accounts by dragging them

☑ Keep favorites at top
    Favorite accounts always appear first

[ Reset to Automatic Sorting ]
```

### 7. Context Menu Addition

Add menu item to reset sorting:
```cpp
// In account list context menu
auto reset_order_item = Gtk::make_managed<Gtk::MenuItem>("Reset to Automatic Sorting");
reset_order_item->signal_activate().connect([this]() {
    auto dialog = Gtk::MessageDialog(
        *this,
        "Reset to automatic sorting?",
        false,
        Gtk::MessageType::QUESTION,
        Gtk::ButtonsType::YES_NO,
        true);

    dialog.set_secondary_text(
        "This will remove custom ordering and sort accounts alphabetically "
        "(with favorites first).");

    if (dialog.run() == Gtk::ResponseType::YES) {
        m_vault_manager->reset_display_order();
        update_account_list();
    }
});

m_context_menu.append(*reset_order_item);
```

## Implementation Phases

### Phase 1: Data Model Foundation (2-3 days)
**Goal**: Prepare protobuf schema for both drag-drop AND future groups

- [ ] Design complete protobuf schema with:
  - [ ] `AccountGroup` message (group_id, name, display_order, is_system_group)
  - [ ] `GroupMembership` message (group_id, display_order within group)
  - [ ] `global_display_order` field in AccountRecord
  - [ ] `repeated GroupMembership groups` field in AccountRecord
- [ ] Regenerate protobuf C++ code
- [ ] Implement Phase 1 VaultManager methods:
  - [ ] `reorder_account()` (uses global_display_order)
  - [ ] `has_custom_global_ordering()`
  - [ ] `reset_global_display_order()`
- [ ] Implement Phase 2 method stubs (for future):
  - [ ] `create_group()`, `delete_group()`
  - [ ] `add_account_to_group()`, `remove_account_from_group()`
  - [ ] `get_favorites_group_id()` (will auto-create "Favorites" system group)
- [ ] Add unit tests for reordering logic
- [ ] Test migration (old vaults still work)

### Phase 2: Drag-and-Drop UI (2-3 days)
**Goal**: Visual drag-drop for global account reordering

- [ ] Implement `Gtk::DragSource` for TreeView rows
- [ ] Implement `Gtk::DropTarget` for TreeView rows
- [ ] Create drag icon (visual feedback)
- [ ] Handle drop position calculation
- [ ] Update sorting logic in `update_account_list()` to use global_display_order
- [ ] Test drag-and-drop interaction

### Phase 3: Undo/Redo Integration (1 day)
**Goal**: Make reordering undoable

- [ ] Create `ReorderAccountCommand` class
- [ ] Integrate with `UndoManager`
- [ ] Update command execution in drop handler
- [ ] Test undo/redo for reordering operations

### Phase 4: Preferences & Polish (1-2 days)
**Goal**: User controls and refinement

- [ ] Add GSettings key `enable-drag-drop-reorder`
- [ ] Update PreferencesDialog UI
- [ ] Add "Reset to Automatic Sorting" context menu item
- [ ] Handle preference changes (enable/disable drag-drop)
- [ ] Disable drag-drop during search/filter
- [ ] Add visual indicators (e.g., drag handle icon)

### Phase 5: Testing & Documentation (1 day)
- [ ] Write comprehensive unit tests
- [ ] Manual testing with various scenarios
- [ ] Update user documentation
- [ ] Create migration guide for existing vaults

## Edge Cases & Considerations

### 1. Favorites Handling (Phase 2 - Groups)
**Chosen Approach**: Favorites as a system group with multi-membership

Phase 1 (Drag-Drop):
- Use current behavior: favorites sort to top, then alphabetical
- `global_display_order` applies to entire list
- Dragging doesn't affect favorite status (star remains)

Phase 2 (Groups):
- "Favorites" becomes a collapsible system group
- Starring an account automatically adds it to "Favorites" group
- Unstarring removes from "Favorites" group
- Account can be in "Favorites" AND other groups simultaneously
- Example: Account can be in both "Favorites" and "Work"

**Key Benefit**: No artificial zones. Accounts can belong to multiple groups naturally.

### 2. Search/Filter Interaction
When search is active:
- Disable drag-and-drop (filtered view, not real order)
- Show message: "Clear search to reorder accounts"
- Prevent confusing behavior

### 3. Multi-Account Selection
For future enhancement:
- Select multiple accounts (Ctrl+Click)
- Drag them as a group
- Maintain relative order within group
- Phase 1: Single account drag only

### 4. Performance
With 1000+ accounts:
- Recomputing display_order for all might be slow
- Solution: Only update affected range (old_index to new_index)
- Current design already handles this efficiently

### 5. Accessibility
- Ensure keyboard-only reordering is possible
- Add keyboard shortcuts (Alt+Up/Down to move account)
- Screen reader support (announce reorder actions)

## Testing Strategy

### Unit Tests
```cpp
TEST(DragDropTest, ReorderAccountDown) {
    // Setup: 5 accounts [A, B, C, D, E]
    // Move B (index 1) to position 3 (after D)
    // Expected: [A, C, D, B, E]

    EXPECT_TRUE(vault_manager->reorder_account(1, 3));

    auto accounts = vault_manager->get_all_accounts();
    EXPECT_EQ(accounts[0].account_name(), "A");
    EXPECT_EQ(accounts[1].account_name(), "C");
    EXPECT_EQ(accounts[2].account_name(), "D");
    EXPECT_EQ(accounts[3].account_name(), "B");
    EXPECT_EQ(accounts[4].account_name(), "E");
}

TEST(DragDropTest, UndoReorder) {
    // Reorder, then undo
    auto command = std::make_unique<ReorderAccountCommand>(vault_manager, 1, 3);
    EXPECT_TRUE(command->execute());
    EXPECT_TRUE(command->undo());

    // Should be back to original order
    auto accounts = vault_manager->get_all_accounts();
    EXPECT_EQ(accounts[1].account_name(), "B");
}

TEST(DragDropTest, ResetDisplayOrder) {
    // Setup custom order, then reset
    vault_manager->reorder_account(0, 2);
    EXPECT_TRUE(vault_manager->has_custom_ordering());

    vault_manager->reset_display_order();
    EXPECT_FALSE(vault_manager->has_custom_ordering());
}
```

### Manual Test Cases
1. **Basic Reorder**: Drag account down, drag account up
2. **Undo/Redo**: Reorder → undo → redo, verify correct order
3. **Multiple Reorders**: Chain of 5+ reorder operations
4. **Edge Positions**: Drag to first position, drag to last position
5. **Favorites**: Reorder within favorites, reorder regular accounts
6. **Save/Load**: Reorder, close vault, reopen → order preserved
7. **Preference Toggle**: Disable drag-drop → UI prevents dragging
8. **Reset Order**: Use menu to reset → returns to alphabetical

## Phase 2 Features (Account Groups - Future Implementation)

Once drag-and-drop foundation is complete, groups will build on top:

### 1. Multi-Group Membership
**Core Functionality**:
- Account can belong to 0, 1, or many groups
- "Favorites" is a system group (auto-managed by star/unstar)
- Users create custom groups (e.g., "Work", "Personal", "Banking")
- Each membership has a `display_order` within that group

### 2. Group Management UI
**Tree View Structure**:
```
📁 Favorites (5 accounts) [expandable]
   ⭐ Account A (order: 0)
   ⭐ Account B (order: 1)
📁 Work (12 accounts) [expandable]
   🏢 Account C (order: 0)
   🏢 Account D (order: 1)
📁 Personal (8 accounts) [collapsed]
📁 All Accounts (25 accounts) [special view - no groups]
   Account A
   Account B
   Account C
   ...
```

### 3. Drag-and-Drop with Groups
**Enhanced Interactions**:
- Drag account to group header → Add to group (multi-membership)
- Drag account within group → Reorder within that group
- Drag group header → Reorder groups
- Drag from one group to another → Add to target group (keeps source membership)
- Context menu: "Remove from this group" (doesn't delete account)

### 4. Favorites Auto-Management
**Starring Behavior**:
```cpp
void MainWindow::on_toggle_favorite() {
    bool is_now_favorite = !account.is_favorite();

    if (is_now_favorite) {
        // Add to "Favorites" system group
        vault_manager->add_account_to_group(index, get_favorites_group_id());
    } else {
        // Remove from "Favorites" system group
        vault_manager->remove_account_from_group(index, get_favorites_group_id());
    }

    account.set_is_favorite(is_now_favorite);  // Keep flag for compatibility
}
```

### 5. Future Enhancements (Phase 3+)
**Advanced Features**:
- Multi-Selection Drag (select multiple accounts, drag as group)
- Keyboard Shortcuts (Alt+Up/Down to move account)
- Visual Drag Handle (grip icon for clearer affordance)
- Smooth Animations (animate row movement)
- Group Icons/Colors (visual customization)
- Nested Groups (groups within groups)

## Dependencies

- GTK4 >= 4.10 (for Gtk::DragSource and Gtk::DropTarget)
- Protobuf >= 3.0 (already required)
- C++23 (already required)

## Estimated Timeline

- **Total Implementation**: 7-10 days
- **Testing**: 2-3 days
- **Documentation**: 1 day
- **Total**: ~2 weeks for complete feature

## Success Criteria

- [ ] Users can drag accounts to reorder them
- [ ] Custom order persists across app restarts
- [ ] Undo/redo works for reordering
- [ ] Preference toggle to enable/disable feature
- [ ] No performance degradation with 500+ accounts
- [ ] All existing tests still pass
- [ ] New tests cover reordering logic
- [ ] No regressions in search/filter behavior
- [ ] Accessibility maintained (keyboard navigation)
