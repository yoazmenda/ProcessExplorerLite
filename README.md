# ProcessExplorerLite

A lightweight, terminal-based process monitoring tool for Linux and macOS. Similar to `top`, built from scratch as a learning project.

## Quick Install

```bash
curl -fsSL https://raw.githubusercontent.com/yoazmenda/ProcessExplorerLite/master/install.sh | bash
```

Then run:
```bash
processexplorer
```

## What It Does

- Real-time process monitoring with color-coded display
- Interactive TUI using ncurses
- Auto-refresh every 1 second
- Responsive keyboard controls

## Keyboard Controls

- `q` - Quit
- `r` - Force refresh

## Supported Platforms

- Linux x86_64
- Linux ARM64
- macOS x86_64 (Intel)
- macOS ARM64 (Apple Silicon)

## Build From Source

**Requirements:**
- GCC compiler
- ncurses library

**On Ubuntu/Debian:**
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

**On macOS:**
```bash
brew install ncurses
```

**Build:**
```bash
git clone https://github.com/yoazmenda/ProcessExplorerLite.git
cd ProcessExplorerLite
make
./processexplorer
```

## Development

```bash
make clean          # Clean build artifacts
make static         # Build static binary for release
sudo make install   # Install to /usr/local/bin
sudo make uninstall # Uninstall
```

## License

MIT License - Free to use and modify.
