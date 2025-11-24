# ProcessExplorerLite

A lightweight, terminal-based process monitoring tool for Linux, similar to `top` but built from scratch as a learning project to explore systems programming concepts.

## Features

- Interactive TUI using ncurses
- Real-time process monitoring
- Responsive keyboard input with no delay
- Color-coded display
- Auto-refresh every 1 second
- Signal handling for window resize (SIGWINCH)

## Installation

### Quick Install

Install the latest release for your architecture:

```bash
curl -fsSL https://raw.githubusercontent.com/yoazmenda/ProcessExplorerLite/master/install.sh | bash
```

### Run Without Installing

Try it out without installing to your system:

```bash
curl -fsSL https://raw.githubusercontent.com/yoazmenda/ProcessExplorerLite/master/install.sh | bash -s -- --run
```

### Build From Source

#### Requirements

- GCC compiler
- ncurses library

On Ubuntu/Debian:
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

#### Building

```bash
git clone https://github.com/yoazmenda/ProcessExplorerLite.git
cd ProcessExplorerLite
make
```

#### Running

```bash
./processexplorer
```

Or build and run in one command:
```bash
make run
```

## Keyboard Controls

- `q` - Quit the application
- `r` - Force refresh
- `h` - Help (coming soon)

## Architecture

The project demonstrates several systems programming concepts:

- **I/O Multiplexing**: Uses `select()` for responsive input without blocking
- **Signal Handling**: Implements SIGWINCH for instant window resize
- **Process Information**: Reads from `/proc` filesystem
- **Terminal Control**: Uses ncurses for TUI rendering

### Code Structure

```
main.c       - Main application and TUI logic
task_data.c  - Process data collection and management
Makefile     - Build configuration
```

## Development

### Clean Build

```bash
make clean
```

### Install System-wide (Optional)

```bash
sudo make install
```

This installs to `/usr/local/bin/processexplorer`.

### Uninstall

```bash
sudo make uninstall
```

## Supported Platforms

- Linux x86_64
- Linux ARM64 (aarch64)

## License

MIT License - Free to use and modify.

## Contributing

This is a personal learning project, but feel free to fork and experiment!
