#!/bin/bash
# Test script to verify KeySlot serialization doesn't include plaintext usernames

cd /home/tjdev/Projects/KeepTower

# Create a test vault with a known username
TEST_VAULT="/tmp/test_username_serialize.vault"
TEST_USER="TestUsernamePlaintext123"

rm -f "$TEST_VAULT"

# Run the binary to create a test vault (this would need manual intervention)
echo "Manual test required: Create a vault with username: $TEST_USER"
echo "Then check the vault file:"
echo ""
echo "xxd $TEST_VAULT | grep -C 5 'TestUsernamePlaintext123'"
echo ""
echo "If you see the username in ASCII, the serialization is broken."
echo "If you only see random bytes (username_hash), serialization is correct."
