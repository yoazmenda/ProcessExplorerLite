#!/bin/sh

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

# Detect OS
detect_os() {
    os=$(uname -s)
    case $os in
        Linux)
            echo "linux"
            ;;
        Darwin)
            echo "darwin"
            ;;
        *)
            echo "${RED}Error: Unsupported OS: $os${NC}" >&2
            echo "Supported OS: Linux, macOS (Darwin)" >&2
            exit 1
            ;;
    esac
}

# Detect architecture
detect_arch() {
    arch=$(uname -m)
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

# Check and install ncurses dependency
ensure_dependencies() {
    # macOS has ncurses built-in, no need to install
    if [ "$(uname -s)" = "Darwin" ]; then
        return 0
    fi

    # Check if ncurses is already available (Linux only)
    if ldconfig -p 2>/dev/null | grep -q libncurses.so; then
        return 0
    fi

    echo "${YELLOW}Installing required dependencies...${NC}"

    # Detect package manager and install ncurses
    if command -v apt-get >/dev/null 2>&1; then
        apt-get update -qq >/dev/null 2>&1 || true
        apt-get install -y -qq libncurses6 >/dev/null 2>&1 || apt-get install -y -qq libncurses5 >/dev/null 2>&1
    elif command -v yum >/dev/null 2>&1; then
        yum install -y -q ncurses-libs >/dev/null 2>&1
    elif command -v apk >/dev/null 2>&1; then
        apk add --no-cache ncurses-libs >/dev/null 2>&1
    else
        echo "${YELLOW}Warning: Could not install ncurses automatically${NC}"
        echo "${YELLOW}Please install ncurses manually if the program fails to run${NC}"
    fi
}

# Main installation function
main() {
    echo "ProcessExplorerLite Installer"
    echo "============================="
    echo

    # Detect OS and architecture
    OS=$(detect_os)
    ARCH=$(detect_arch)
    echo "Detected platform: $OS-$ARCH"

    # Get latest version
    echo "Fetching latest release..."
    VERSION=$(get_latest_version)

    if [ -z "$VERSION" ]; then
        echo "${RED}Error: Could not fetch latest release version${NC}" >&2
        exit 1
    fi

    echo "Latest version: $VERSION"

    # Construct download URL
    DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$BINARY_NAME-$OS-$ARCH"

    # Create temporary directory
    TMP_DIR=$(mktemp -d)
    trap "rm -rf $TMP_DIR" EXIT

    # Download binary
    echo "Downloading $BINARY_NAME-$OS-$ARCH..."
    if ! curl -fsSL "$DOWNLOAD_URL" -o "$TMP_DIR/$BINARY_NAME"; then
        echo "${RED}Error: Failed to download binary${NC}" >&2
        echo "URL: $DOWNLOAD_URL" >&2
        exit 1
    fi

    # Make executable
    chmod +x "$TMP_DIR/$BINARY_NAME"

    # Ensure dependencies are installed
    ensure_dependencies

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
}

main
