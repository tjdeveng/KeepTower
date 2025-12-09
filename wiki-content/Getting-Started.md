# Getting Started

This guide will walk you through creating your first vault and adding your first password.

## First Launch

When you launch KeepTower for the first time, you'll see an empty main window with no vault open.

## Creating Your First Vault

### Step 1: Create New Vault

1. Click **File** → **New Vault** (or press `Ctrl+N`)
2. Choose a location and filename for your vault
   - Example: `~/Documents/passwords.vault`
   - The `.vault` extension is added automatically

### Step 2: Set Master Password

A dialog will appear asking you to create a master password. This is the **most important password** you'll ever create for KeepTower.

**Master Password Guidelines:**
- ✅ At least 12 characters long
- ✅ Mix of uppercase, lowercase, numbers, and symbols
- ✅ Not a common word or pattern
- ✅ Unique to KeepTower (don't reuse elsewhere)
- ✅ Memorable but strong

**⚠️ IMPORTANT:** There is **no password recovery**. If you forget your master password, your vault cannot be opened. Write it down and store it securely if needed.

The password strength indicator will help you create a strong password:
- 🔴 **Weak** - Too short or common
- 🟡 **Fair** - Acceptable but could be stronger
- 🟢 **Strong** - Good password
- 🔵 **Very Strong** - Excellent password

### Step 3: Confirm Password

Re-enter your master password to confirm. Both entries must match exactly.

Click **Create** and your vault will be created and opened.

---

## Understanding the Interface

### Main Window Components

```
┌─────────────────────────────────────────┐
│ File  Edit  View  Help                  │  ← Menu Bar
├─────────────────────────────────────────┤
│ [🔍 Search...]                    [+]   │  ← Search & Add
├─────────────┬───────────────────────────┤
│             │                           │
│  Account    │    Account Details        │
│  List       │    ────────────────       │
│             │    Name: example.com      │
│  □ Example  │    Username: user@mail    │
│  □ Work     │    Password: ••••••••     │
│  □ Email    │    URL: https://...       │
│             │    [Show] [Copy]          │
│             │                           │
└─────────────┴───────────────────────────┘
```

- **Left Panel:** List of all accounts in your vault
- **Right Panel:** Details of the selected account
- **Search Bar:** Quickly find accounts by name or username
- **Add Button (+):** Create new account entries

---

## Adding Your First Password

### Step 1: Click Add Account

Click the **+** button in the toolbar or press `Ctrl+A`.

### Step 2: Fill in Account Details

**Required Fields:**
- **Account Name:** Descriptive name (e.g., "Gmail", "GitHub", "Banking")
- **Password:** The password for this account

**Optional Fields:**
- **Username:** Your username or email for this account
- **URL:** Website URL (e.g., https://example.com)

### Step 3: Choose a Strong Password

You can:
- **Enter an existing password** - If you're saving a password you already have
- **Use the password generator** - Click "Generate" for a secure random password

### Step 4: Save

Click **Add** to save the account to your vault.

---

## Using Stored Passwords

### Viewing a Password

1. Select an account from the list
2. Click the **Show** button (eye icon) to reveal the password
3. Click again to hide it

### Copying a Password

1. Select an account from the list
2. Click **Copy Password** button
3. The password is copied to your clipboard
4. Paste it where needed (`Ctrl+V`)

**Security Note:** The password is automatically cleared from your clipboard after 45 seconds.

---

## Organizing Accounts

### Searching

Type in the search bar to filter accounts by:
- Account name
- Username
- URL

The list updates in real-time as you type.

### Editing Accounts

1. Select an account
2. Click **Edit** button
3. Modify any fields
4. Click **Save**

### Deleting Accounts

1. Select an account
2. Click **Delete** button
3. Confirm the deletion

**⚠️ Warning:** Deleted accounts cannot be recovered (unless you have backups enabled).

---

## Saving Your Vault

KeepTower automatically marks the vault as modified when you make changes. You can:

- **Manually Save:** Click **File** → **Save** or press `Ctrl+S`
- **Auto-save:** Changes are saved when you close the vault or application

---

## Closing and Opening Vaults

### Closing a Vault

- Click **File** → **Close Vault** or press `Ctrl+W`
- You'll be prompted to save if there are unsaved changes

### Opening an Existing Vault

1. Click **File** → **Open Vault** or press `Ctrl+O`
2. Select your `.vault` file
3. Enter your master password
4. Click **Open**

---

## Configuring Preferences

Access preferences via **Edit** → **Preferences** or press `Ctrl+,`.

### Reed-Solomon Error Correction

**What is it?** FEC adds redundancy to protect your vault from data corruption (bit rot, bad sectors, etc.).

**Settings:**
- **Enable Reed-Solomon:** Check to enable error correction
- **Redundancy Percentage:** 5-50% (higher = more protection, larger file)
  - **10%** - Good balance (recommended)
  - **25%** - High protection
  - **50%** - Maximum protection

**Apply to current vault:** Check this to change settings for the currently open vault. Uncheck to only change defaults for new vaults.

### Backups

- **Enable Automatic Backups:** Creates backups before saving
- **Number of Backups:** How many backup copies to keep (1-10)
- Backups are stored next to your vault file with timestamps

### Theme

- **System Default:** Follow desktop theme
- **Light:** Always use light theme
- **Dark:** Always use dark theme

---

## Security Best Practices

1. ✅ **Use a strong master password** - This protects everything
2. ✅ **Enable backups** - Protects against accidental deletion or corruption
3. ✅ **Enable Reed-Solomon FEC** - Protects against bit rot
4. ✅ **Store vault on reliable storage** - Use SSD or regularly backed-up drive
5. ✅ **Don't share your master password** - Ever
6. ✅ **Close vault when not in use** - Keeps it encrypted
7. ✅ **Keep KeepTower updated** - Security patches and improvements

---

## What's Next?

- **[[User Guide]]** - Learn about all features in detail
- **[[Security]]** - Understand how KeepTower protects your data
- **[[FAQ]]** - Common questions and answers

**Need help?** Open an issue on [GitHub](https://github.com/tjdeveng/KeepTower/issues).
