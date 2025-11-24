#!/bin/bash

set -e

# Configuration
REPO="yoazmenda/ProcessExplorerLite"
BINARY_NAME="processexplorer"
INSTALL_DIR="/usr/local/bin"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Parse arguments
RUN_ONLY=false
if [ "$1" = "--run" ]; then
    RUN_ONLY=true
fi

# Detect architecture
detect_arch() {
    local arch=$(uname -m)
    case $arch in
        x86_64)
            echo "x86_64"
            ;;
        aarch64|arm64)
            echo "aarch64"
            ;;
        *)
            echo "${RED}Error: Unsupported architecture: $arch${NC}" >&2
            echo "Supported architectures: x86_64, aarch64/arm64" >&2
            exit 1
            ;;
    esac
}

# Get latest release version
get_latest_version() {
    curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/'
}

# Main installation function
main() {
    echo "ProcessExplorerLite Installer"
    echo "============================="
    echo

    # Detect architecture
    ARCH=$(detect_arch)
    echo "Detected architecture: $ARCH"

    # Get latest version
    echo "Fetching latest release..."
    VERSION=$(get_latest_version)

    if [ -z "$VERSION" ]; then
        echo "${RED}Error: Could not fetch latest release version${NC}" >&2
        exit 1
    fi

    echo "Latest version: $VERSION"

    # Construct download URL
    DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$BINARY_NAME-$ARCH"

    # Create temporary directory
    TMP_DIR=$(mktemp -d)
    trap "rm -rf $TMP_DIR" EXIT

    # Download binary
    echo "Downloading $BINARY_NAME-$ARCH..."
    if ! curl -fsSL "$DOWNLOAD_URL" -o "$TMP_DIR/$BINARY_NAME"; then
        echo "${RED}Error: Failed to download binary${NC}" >&2
        echo "URL: $DOWNLOAD_URL" >&2
        exit 1
    fi

    # Make executable
    chmod +x "$TMP_DIR/$BINARY_NAME"

    if [ "$RUN_ONLY" = true ]; then
        # Run directly without installing
        echo "${GREEN}Running $BINARY_NAME...${NC}"
        exec "$TMP_DIR/$BINARY_NAME"
    else
        # Install to system
        echo "Installing to $INSTALL_DIR/$BINARY_NAME..."

        if [ -w "$INSTALL_DIR" ]; then
            mv "$TMP_DIR/$BINARY_NAME" "$INSTALL_DIR/$BINARY_NAME"
        else
            echo "${YELLOW}Note: Installing to $INSTALL_DIR requires sudo${NC}"
            sudo mv "$TMP_DIR/$BINARY_NAME" "$INSTALL_DIR/$BINARY_NAME"
        fi

        echo "${GREEN}Installation complete!${NC}"
        echo
        echo "Run with: $BINARY_NAME"
        echo "Or: $INSTALL_DIR/$BINARY_NAME"
    fi
}

main
