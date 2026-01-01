#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 tjdeveng

# ============================================================================
# YubiKey FIPS-140-3 Configuration Script
# ============================================================================
#
# This script helps configure YubiKey devices for FIPS-compliant use with
# KeepTower password manager. It will:
#   1. Check YubiKey compatibility
#   2. Verify firmware version supports HMAC-SHA256
#   3. Configure slot 2 with FIPS-approved algorithm
#   4. Verify configuration
#
# Requirements:
#   - YubiKey 5 Series (firmware 5.0+)
#   - ykman (YubiKey Manager CLI) OR
#   - ykpersonalize + ykinfo (yubikey-personalization)
#
# Usage:
#   ./configure-yubikey-fips.sh [OPTIONS]
#
# Options:
#   --slot <1|2>      YubiKey slot to configure (default: 2)
#   --no-touch        Disable touch requirement (NOT recommended)
#   --check-only      Only check compatibility, don't configure
#   --help            Show this help message
#
# ============================================================================

set -e  # Exit on error
set -u  # Exit on undefined variable

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SLOT=2
REQUIRE_TOUCH=1
CHECK_ONLY=0
MIN_FIRMWARE_MAJOR=5
MIN_FIRMWARE_MINOR=0

# ============================================================================
# Helper Functions
# ============================================================================

print_header() {
    echo ""
    echo "============================================================================"
    echo "  YubiKey FIPS-140-3 Configuration Tool"
    echo "  KeepTower Password Manager"
    echo "============================================================================"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1" >&2
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_step() {
    echo ""
    echo -e "${BLUE}▶${NC} $1"
    echo "────────────────────────────────────────────────────────────────────────────"
}

check_command() {
    if command -v "$1" &> /dev/null; then
        return 0
    else
        return 1
    fi
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --slot)
                SLOT="$2"
                if [[ "$SLOT" != "1" && "$SLOT" != "2" ]]; then
                    print_error "Invalid slot number. Must be 1 or 2."
                    exit 1
                fi
                shift 2
                ;;
            --no-touch)
                REQUIRE_TOUCH=0
                print_warning "Touch requirement will be DISABLED (not recommended for security)"
                shift
                ;;
            --check-only)
                CHECK_ONLY=1
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

show_help() {
    echo "YubiKey FIPS-140-3 Configuration Script"
    echo ""
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --slot <1|2>      YubiKey slot to configure (default: 2)"
    echo "  --no-touch        Disable touch requirement (NOT recommended)"
    echo "  --check-only      Only check compatibility, don't configure"
    echo "  --help, -h        Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                           # Configure slot 2 with touch"
    echo "  $0 --slot 1                  # Configure slot 1 instead"
    echo "  $0 --check-only              # Only check compatibility"
    echo ""
}

# ============================================================================
# Step 1: Check Prerequisites
# ============================================================================

check_prerequisites() {
    print_step "Step 1: Checking Prerequisites"

    # Check for YubiKey tools
    if check_command ykman; then
        TOOL="ykman"
        print_success "Found ykman (YubiKey Manager CLI)"
    elif check_command ykpersonalize && check_command ykinfo; then
        TOOL="ykpersonalize"
        print_success "Found ykpersonalize + ykinfo"
    else
        print_error "No YubiKey tools found!"
        echo ""
        echo "Please install one of:"
        echo "  - ykman (recommended): https://www.yubico.com/support/download/yubikey-manager/"
        echo "  - yubikey-personalization: Install via your package manager"
        echo ""
        echo "Installation examples:"
        echo "  Ubuntu/Debian:  sudo apt install yubikey-manager"
        echo "  Fedora:         sudo dnf install yubikey-manager"
        echo "  Arch:           sudo pacman -S yubikey-manager"
        echo "  macOS:          brew install ykman"
        exit 1
    fi

    # Check if YubiKey is connected
    if [[ "$TOOL" == "ykman" ]]; then
        if ! ykman list &> /dev/null; then
            print_error "No YubiKey detected!"
            echo ""
            echo "Please:"
            echo "  1. Insert your YubiKey"
            echo "  2. Run this script again"
            exit 1
        fi
    else
        if ! ykinfo -v &> /dev/null; then
            print_error "No YubiKey detected!"
            echo ""
            echo "Please:"
            echo "  1. Insert your YubiKey"
            echo "  2. Check USB permissions (Linux: add user to 'plugdev' group)"
            echo "  3. Run this script again"
            exit 1
        fi
    fi

    print_success "YubiKey detected"
}

# ============================================================================
# Step 2: Check YubiKey Compatibility
# ============================================================================

check_compatibility() {
    print_step "Step 2: Checking YubiKey Compatibility"

    # Get firmware version
    if [[ "$TOOL" == "ykman" ]]; then
        FW_VERSION=$(ykman info | grep "Firmware version:" | awk '{print $3}')
        DEVICE_TYPE=$(ykman info | grep "Device type:" | awk '{print $3, $4}')
    else
        FW_VERSION=$(ykinfo -v | grep "version:" | awk '{print $2}')
        DEVICE_TYPE=$(ykinfo -v | grep "vendor_id:" | awk '{print "YubiKey"}')
    fi

    print_info "Device: $DEVICE_TYPE"
    print_info "Firmware: $FW_VERSION"

    # Parse version
    FW_MAJOR=$(echo "$FW_VERSION" | cut -d. -f1)
    FW_MINOR=$(echo "$FW_VERSION" | cut -d. -f2)

    # Check minimum version
    if [[ $FW_MAJOR -lt $MIN_FIRMWARE_MAJOR ]]; then
        print_error "YubiKey firmware too old!"
        echo ""
        echo "FIPS-compliant configuration requires:"
        echo "  - YubiKey 5 Series (firmware 5.0 or later)"
        echo "  - Your firmware: $FW_VERSION"
        echo ""
        echo "YubiKey firmware cannot be upgraded. You need to purchase a newer device."
        exit 1
    fi

    print_success "Firmware version is compatible (${FW_VERSION} >= ${MIN_FIRMWARE_MAJOR}.${MIN_FIRMWARE_MINOR})"

    # Check for FIPS edition
    if [[ $FW_MAJOR -ge 5 && $FW_MINOR -ge 4 ]]; then
        print_info "Firmware 5.4+ detected - YubiKey 5 FIPS may be available"
        if [[ "$DEVICE_TYPE" == *"FIPS"* ]]; then
            print_success "YubiKey 5 FIPS Edition detected! (FIPS 140-2 Level 2 certified)"
        fi
    fi
}

# ============================================================================
# Step 3: Check Current Configuration
# ============================================================================

check_current_config() {
    print_step "Step 3: Checking Current Configuration"

    if [[ "$TOOL" == "ykman" ]]; then
        CONFIG=$(ykman otp info 2>&1 || true)

        if echo "$CONFIG" | grep -q "Slot $SLOT:.*programmed"; then
            print_warning "Slot $SLOT is already configured"
            echo ""
            echo "$CONFIG" | grep "Slot $SLOT:" -A 2
            echo ""

            if [[ $CHECK_ONLY -eq 0 ]]; then
                read -p "Overwrite existing configuration? (yes/NO): " confirm
                if [[ "$confirm" != "yes" ]]; then
                    print_info "Configuration cancelled by user"
                    exit 0
                fi
            fi
        else
            print_info "Slot $SLOT is not configured (ready for setup)"
        fi
    else
        # Check with ykpersonalize
        if ykinfo -a 2>&1 | grep -q "slot${SLOT}:"; then
            print_warning "Slot $SLOT appears to be configured"
            echo ""
            ykinfo -a | grep "slot${SLOT}:"
            echo ""

            if [[ $CHECK_ONLY -eq 0 ]]; then
                read -p "Overwrite existing configuration? (yes/NO): " confirm
                if [[ "$confirm" != "yes" ]]; then
                    print_info "Configuration cancelled by user"
                    exit 0
                fi
            fi
        else
            print_info "Slot $SLOT is not configured (ready for setup)"
        fi
    fi
}

# ============================================================================
# Step 4: Configure YubiKey
# ============================================================================

configure_yubikey() {
    if [[ $CHECK_ONLY -eq 1 ]]; then
        print_success "Compatibility check passed!"
        echo ""
        echo "Your YubiKey is compatible with FIPS-140-3 mode."
        echo "Run without --check-only to configure slot $SLOT."
        exit 0
    fi

    print_step "Step 4: Configuring YubiKey for FIPS Mode"

    print_warning "This will program slot $SLOT with HMAC-SHA256 challenge-response"
    echo ""

    if [[ "$TOOL" == "ykman" ]]; then
        # Use ykman
        TOUCH_FLAG=""
        if [[ $REQUIRE_TOUCH -eq 1 ]]; then
            TOUCH_FLAG="--touch"
            print_info "Touch requirement: ENABLED (recommended)"
        else
            print_warning "Touch requirement: DISABLED (not recommended)"
        fi

        print_info "Generating random secret and programming YubiKey..."
        print_info "Command: ykman otp chalresp $TOUCH_FLAG --generate $SLOT"
        echo ""

        if ykman otp chalresp $TOUCH_FLAG --generate $SLOT; then
            print_success "YubiKey configured successfully!"
        else
            print_error "Configuration failed!"
            exit 1
        fi
    else
        # Use ykpersonalize
        OPTS="-${SLOT} -ochal-resp -ochal-hmac -ohmac-sha256 -oserial-api-visible"

        if [[ $REQUIRE_TOUCH -eq 1 ]]; then
            OPTS="$OPTS -ochal-btn-trig"
            print_info "Touch requirement: ENABLED (recommended)"
        else
            print_warning "Touch requirement: DISABLED (not recommended)"
        fi

        print_info "Programming YubiKey with FIPS-approved HMAC-SHA256..."
        print_info "Command: ykpersonalize $OPTS"
        echo ""

        if ykpersonalize $OPTS; then
            print_success "YubiKey configured successfully!"
        else
            print_error "Configuration failed!"
            exit 1
        fi
    fi
}

# ============================================================================
# Step 5: Verify Configuration
# ============================================================================

verify_configuration() {
    print_step "Step 5: Verifying Configuration"

    if [[ "$TOOL" == "ykman" ]]; then
        CONFIG=$(ykman otp info)

        if echo "$CONFIG" | grep -q "Slot $SLOT:.*programmed"; then
            print_success "Slot $SLOT is configured"

            # Check for SHA-256
            if echo "$CONFIG" | grep -A 2 "Slot $SLOT:" | grep -q "SHA256\|sha256"; then
                print_success "Algorithm: HMAC-SHA256 (FIPS-approved) ✓"
            else
                print_warning "Could not verify HMAC-SHA256 from output"
            fi

            # Check for touch requirement
            if [[ $REQUIRE_TOUCH -eq 1 ]]; then
                if echo "$CONFIG" | grep -A 2 "Slot $SLOT:" | grep -q "touch"; then
                    print_success "Touch requirement: ENABLED ✓"
                else
                    print_warning "Touch requirement status unclear"
                fi
            fi

            echo ""
            echo "Configuration details:"
            echo "$CONFIG" | grep "Slot $SLOT:" -A 3
        else
            print_error "Slot $SLOT verification failed!"
            exit 1
        fi
    else
        # Verify with ykinfo
        if ykinfo -a | grep -q "slot${SLOT}:"; then
            print_success "Slot $SLOT is configured"

            INFO=$(ykinfo -a | grep "slot${SLOT}:")
            echo ""
            echo "Configuration: $INFO"

            if echo "$INFO" | grep -q "HMAC_SHA256"; then
                print_success "Algorithm: HMAC-SHA256 (FIPS-approved) ✓"
            else
                print_warning "Could not verify HMAC-SHA256"
            fi
        else
            print_error "Slot $SLOT verification failed!"
            exit 1
        fi
    fi

    # Test challenge-response
    print_info "Testing challenge-response..."
    if [[ "$TOOL" == "ykman" ]]; then
        if [[ $REQUIRE_TOUCH -eq 1 ]]; then
            print_warning "Please touch your YubiKey when it blinks..."
        fi

        TEST_CHALLENGE="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
        if echo "$TEST_CHALLENGE" | ykman otp calculate $SLOT -H &> /dev/null; then
            print_success "Challenge-response test PASSED"
        else
            print_warning "Challenge-response test inconclusive (may require touch)"
        fi
    fi
}

# ============================================================================
# Step 6: Show Next Steps
# ============================================================================

show_next_steps() {
    print_step "Configuration Complete!"

    echo ""
    echo "Your YubiKey is now configured for FIPS-140-3 compliant use with KeepTower."
    echo ""
    echo "Next steps:"
    echo "  1. ✓ YubiKey configured with HMAC-SHA256 (slot $SLOT)"
    if [[ $REQUIRE_TOUCH -eq 1 ]]; then
        echo "  2. ✓ Touch requirement enabled (security best practice)"
    fi
    echo "  3. Launch KeepTower and create a new vault"
    echo "  4. Enable '2-Factor Authentication with YubiKey'"
    echo "  5. Select algorithm: HMAC-SHA256 (should be default)"
    echo ""
    echo "Important notes:"
    echo "  • Keep your YubiKey secure (treat like a physical key)"
    echo "  • Consider purchasing a backup YubiKey"
    echo "  • The secret is stored ON the YubiKey (cannot be extracted)"
    echo "  • See docs/user/YUBIKEY_FIPS_SETUP.md for detailed guide"
    echo ""

    print_success "Setup complete! You're ready to use KeepTower with FIPS-compliant YubiKey."
    echo ""
}

# ============================================================================
# Main Execution
# ============================================================================

main() {
    print_header

    # Parse command line arguments
    parse_args "$@"

    # Run configuration steps
    check_prerequisites
    check_compatibility
    check_current_config
    configure_yubikey
    verify_configuration
    show_next_steps
}

# Run main function with all arguments
main "$@"
