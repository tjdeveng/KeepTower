#!/bin/bash

# Test script for lock dialog
# Opens a vault and waits for auto-lock after 60 seconds

echo "Starting KeepTower..."
echo "Auto-lock is set to 60 seconds."
echo ""
echo "Steps:"
echo "1. Use File > Open Vault to open 'Untitled.vault'"
echo "2. Enter the password to unlock the vault"
echo "3. Don't interact with the app for 60 seconds"
echo "4. The lock dialog should appear automatically"
echo ""
echo "Expected behavior:"
echo "- Lock dialog appears after 60 seconds of inactivity"
echo "- You should be able to type the password"
echo "- Clicking OK (or pressing Enter) should unlock if password is correct"
echo "- Clicking Cancel should save and close the app"
echo ""
echo "Press Ctrl+C to exit when done testing."
echo ""

# Run with debug messages enabled
G_MESSAGES_DEBUG=all ../../build/src/keeptower
