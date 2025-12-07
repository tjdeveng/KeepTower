#!/bin/bash
# Test script to verify Reed-Solomon preferences
# Usage: ./test_preferences.sh

export GSETTINGS_SCHEMA_DIR="$PWD/data"

echo "Testing Reed-Solomon preferences..."
echo

# Show current settings
echo "Current settings:"
gsettings --schemadir=data get com.tjdeveng.keeptower use-reed-solomon
gsettings --schemadir=data get com.tjdeveng.keeptower rs-redundancy-percent
echo

# Enable RS with 20% redundancy
echo "Enabling Reed-Solomon with 20% redundancy..."
gsettings --schemadir=data set com.tjdeveng.keeptower use-reed-solomon true
gsettings --schemadir=data set com.tjdeveng.keeptower rs-redundancy-percent 20
echo

# Show updated settings
echo "Updated settings:"
gsettings --schemadir=data get com.tjdeveng.keeptower use-reed-solomon
gsettings --schemadir=data get com.tjdeveng.keeptower rs-redundancy-percent
echo

echo "Done! Launch KeepTower with: GSETTINGS_SCHEMA_DIR=\$PWD/data ./build/src/keeptower"
