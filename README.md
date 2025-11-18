# ProcessExplorerLite

> An educational journey into Linux systems programming and OS fundamentals

## 🎯 Project Purpose

This is an **interview preparation project** designed for hands-on learning of low-level systems programming concepts. We're building a process monitoring tool (similar to `top`) from scratch, but the real goal is **understanding how Linux works under the hood**.

### Dual Learning Objectives

1. **Operating System Concepts**
   - Process management and scheduling
   - The `/proc` filesystem interface
   - Memory management
   - System resource monitoring

2. **Linux Systems Programming**
   - POSIX system calls and APIs
   - C programming patterns and best practices
   - File descriptors and I/O multiplexing
   - Signal handling and async events
   - Compilation, headers, and dependencies

### Learning Philosophy

**"Raw over convenient"** - We deliberately choose low-level implementations to maximize learning opportunities. For example, we use `select()` directly instead of ncurses convenience functions, because understanding I/O multiplexing is valuable interview knowledge.

**"Getting stuck is progress"** - When we encounter delays, bugs, or complexity, we treat them as learning opportunities. A 1-second input delay became a gateway to understanding blocking I/O, file descriptors, and the C10K problem!

**Interactive mentoring approach** - This is a 1-on-1 tutoring project. We explore concepts together, ask questions, and build understanding through hands-on practice.

## 📚 Learning Journey

### Session 2 - 2025-11-18 - Signal Handling & Window Resize
**Focus:** SIGWINCH signal handling for responsive terminal resizing
**Status:** 🔄 In Progress
[Detailed notes](docs/sessions/session-02.md) (coming soon)

### Session 1 - 2025-11-17 - Responsive Input with select()
**Problem:** User input had 1-second delay due to `sleep()` in main loop
**Solution:** Implemented `select()` with timeout for I/O multiplexing
**Key Concepts Learned:**
- Blocking vs non-blocking I/O
- File descriptors and streaming I/O
- The `select()` system call and `fd_set` operations
- The C10K problem and history of async I/O
- Opaque types pattern in C APIs

**Outcome:** ✅ Input now responds instantly while maintaining 1-second auto-refresh
[Detailed notes](docs/sessions/session-01.md)

## 🚀 Current Status

**What's Working:**
- ✅ Interactive TUI using ncurses
- ✅ Responsive keyboard input (no delay!)
- ✅ Color support and clean UI layout
- ✅ Auto-refresh every 1 second
- ✅ Basic controls: `q` (quit), `r` (refresh), `h` (help - TODO)

**Current Limitations:**
- ⏳ Window resize has ~1 second delay (fixing next with SIGWINCH)
- 📝 No actual process data yet (placeholder content)
- 📝 No scrolling or sorting yet

**Next Up:**
- 🎯 Implement SIGWINCH signal handler for instant resize response
- 🎯 Add error handling for interrupted system calls (EINTR)
- 🎯 Begin reading process data from `/proc` filesystem

## 🛠️ Development

### Requirements

- GCC compiler
- ncurses library

Install on Ubuntu/Debian:
```bash
sudo apt-get install libncurses5-dev libncursesw5-dev
```

### Building

```bash
make
```

### Running

```bash
./processexplorer
```

Or build and run in one command:
```bash
make run
```

### Cleaning

```bash
make clean
```

## ⌨️ Keyboard Controls

- `q` - Quit the application
- `r` - Force refresh
- `h` - Help (coming soon)

## 📖 Code Structure

```
main.c - Main application with TUI logic
  ├─ init_ui()       - ncurses initialization
  ├─ cleanup_ui()    - cleanup and restore terminal
  ├─ draw_header()   - render title bar
  ├─ draw_footer()   - render help/status bar
  ├─ handle_input()  - keyboard input handling
  └─ main()          - event loop with select()
```

### Key Implementation Details

**Event Loop Pattern:**
```c
while (running) {
    // Reinitialize on each iteration (select modifies them!)
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 1;

    // Draw UI
    clear();
    draw_header();
    draw_footer();
    refresh();

    // Wait for input OR timeout (1 second)
    int result = select(1, &readfds, NULL, NULL, &timeout);

    if (result > 0) {
        ch = getch();
        handle_input(ch);
    }
    // result == 0 means timeout - just loop and redraw
}
```

## 🎓 Interview Prep Notes

This project touches on key systems programming interview topics:

- **I/O Multiplexing:** select(), poll(), epoll()
- **Signal Handling:** Async notifications, signal-safe code
- **Process Management:** /proc filesystem, process states
- **System Calls:** Direct POSIX API usage
- **C Programming:** Memory management, opaque types, error handling

## 📝 License

Educational project - free to use and modify.

---

**Current Branch:** `make_ui_interactive`
**Last Updated:** 2025-11-18
