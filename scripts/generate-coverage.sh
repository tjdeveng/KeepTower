#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Generate code coverage reports using lcov/gcov

set -e

BUILD_DIR="${1:-build-coverage}"
COVERAGE_DIR="${BUILD_DIR}/coverage"

echo "================================"
echo "Code Coverage Report Generator"
echo "================================"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist"
    echo "Please build with coverage enabled first:"
    echo "  meson setup $BUILD_DIR -Dcoverage=true"
    echo "  meson test -C $BUILD_DIR"
    exit 1
fi

# Check for lcov
if ! command -v lcov &> /dev/null; then
    echo "Error: lcov is not installed"
    echo "Install it with:"
    echo "  Ubuntu: sudo apt-get install lcov"
    echo "  Fedora: sudo dnf install lcov"
    exit 1
fi

# Check for genhtml
if ! command -v genhtml &> /dev/null; then
    echo "Error: genhtml is not installed (should come with lcov)"
    exit 1
fi

echo "Build directory: $BUILD_DIR"
echo "Coverage output: $COVERAGE_DIR"
echo ""

# Create coverage directory
mkdir -p "$COVERAGE_DIR"

# Capture coverage data
echo "Step 1: Capturing coverage data..."
lcov --capture \
     --directory "$BUILD_DIR" \
     --output-file "$COVERAGE_DIR/coverage.info" \
     --rc branch_coverage=1 \
     --ignore-errors mismatch,negative \
     2>&1 | grep -v "ignoring data for external file" | tail -10

# Remove system headers and test files from coverage
echo ""
echo "Step 2: Filtering coverage data..."
lcov --remove "$COVERAGE_DIR/coverage.info" \
     '/usr/*' \
     '*/tests/*' \
     '*/build/*' \
     '*/build-coverage/*' \
     '*/meson-*' \
     '*.pb.h' \
     '*.pb.cc' \
     --output-file "$COVERAGE_DIR/coverage-filtered.info" \
     --rc branch_coverage=1 \
     --ignore-errors empty,mismatch \
     2>&1 | grep -v "ignoring data for external file" | tail -10

# Generate HTML report
echo ""
echo "Step 3: Generating HTML report..."
genhtml "$COVERAGE_DIR/coverage-filtered.info" \
        --output-directory "$COVERAGE_DIR/html" \
        --title "KeepTower Code Coverage" \
        --legend \
        --show-details \
        --branch-coverage \
        --rc branch_coverage=1 \
        --ignore-errors empty,mismatch

# Generate summary
echo ""
echo "Step 4: Generating coverage summary..."
if [ -f "$COVERAGE_DIR/coverage-filtered.info" ]; then
    lcov --summary "$COVERAGE_DIR/coverage-filtered.info" \
         --rc branch_coverage=1 \
         --ignore-errors empty \
         2>&1 | grep -E "(lines|functions|branches)" | sed 's/^/  /'
else
    echo "  Warning: coverage-filtered.info not found, skipping summary"
fi
     2>&1 | grep -E "(lines|functions|branches)" | sed 's/^/  /'

echo ""
echo "================================"
echo "Coverage report generated successfully!"
echo ""
echo "View HTML report:"
echo "  xdg-open $COVERAGE_DIR/html/index.html"
echo ""
echo "Or on CI, the coverage-filtered.info can be uploaded to Codecov/Coveralls"
echo "================================"
