# ProcessExplorerLite

An interactive Linux process explorer with a text-based user interface (TUI), similar to `top`.

## Features

- Interactive TUI using ncurses
- Clean, beginner-friendly C code
- Easy to build and extend

## Requirements

- GCC compiler
- ncurses library

To install ncurses on Ubuntu/Debian:
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

## Building

Simply run:
```bash
make
```

## Running

After building:
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
- `h` - Help (TODO)

## Development

### Clean build artifacts
```bash
make clean
```

### Code Structure

- `main.c` - Main application file with TUI logic
- `Makefile` - Build configuration

### Extending the Application

The code is structured with clear sections:
1. **UI Initialization** (`init_ui`) - Sets up ncurses
2. **Drawing Functions** (`draw_header`, `draw_footer`) - Render UI components
3. **Input Handling** (`handle_input`) - Process keyboard input
4. **Main Loop** - Coordinates updates and rendering

Add your process monitoring logic in the main loop where indicated by TODO comments.

## License

Free to use and modify.
