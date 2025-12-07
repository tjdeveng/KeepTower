#!/bin/bash
# Convenience script to run KeepTower with local GSettings schema

cd "$(dirname "$0")"
GSETTINGS_SCHEMA_DIR=$PWD/data ./build/src/keeptower "$@"
