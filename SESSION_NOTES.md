# ProcessExplorerLite - Development Session Notes

## Session Date: 2025-11-17

### Session Goals
- Fix the 1-second input delay problem in the TUI
- Learn about blocking vs non-blocking I/O
- Implement select() for responsive input handling
- Prepare for low-level systems programming interview

---

## Key Concepts Learned

### 1. Blocking vs Non-Blocking I/O

**File Descriptors & Categories:**
- Everything in Unix is a file (actual files, sockets, terminals)
- Two categories of files:
  1. **Streaming I/O** (sockets, pipes, terminals): data arrives unpredictably
  2. **Regular files** (disk files): data is "there", just needs fetching

**Blocking Mode (default):**
- When read() is called and no data available: thread sleeps
- Kernel registers thread as "waiter" on the buffer
- Thread wakes when data arrives
- From user perspective: read() just takes a while

**Non-Blocking Mode:**
- Set with fcntl(fd, F_SETFL, O_NONBLOCK)
- If no data available: returns immediately with EAGAIN/EWOULDBLOCK
- User code must poll/retry later

**Important Discovery:** O_NONBLOCK doesn't work the same for regular disk files!
- Regular files always return "ready" to select/epoll
- Reads may still block if data not in page cache
- Disk I/O blocking is considered "brief enough" to not need async handling
- Modern solution: io_uring (2019) for true async disk I/O

### 2. The C10K Problem & History of select()

**Timeline:**
- 1970s: Everything blocking, one process per task
- 1983: BSD introduces select() for network multiplexing
- Problem: How to handle 10,000 concurrent connections without 10,000 threads?
- Thread-per-connection doesn't scale: 10K threads × 8MB stack = 80GB RAM!
- 2002: Linux adds epoll() for better scalability
- 2009: Node.js popularizes async/event-loop pattern
- 2019: io_uring enables true async disk I/O

**Why select() wasn't designed for disk files:**
- Network sockets: potentially 10,000+ concurrent connections
- Disk files: rarely need 1,000+ concurrent reads
- Threads work fine for disk I/O (small numbers, brief waits)
- So kernel developers never bothered making O_NONBLOCK work properly for disk files

### 3. The select() System Call

**Purpose:** Wait for multiple file descriptors to become ready

**API:**
```c
#include <sys/select.h>
#include <sys/time.h>

int select(int nfds,                  // highest fd + 1
           fd_set *readfds,           // FDs to watch for reading
           fd_set *writefds,          // FDs to watch for writing
           fd_set *exceptfds,         // FDs to watch for exceptions
           struct timeval *timeout);  // timeout or NULL for infinite
```

**fd_set Macros (opaque type pattern):**
```c
FD_ZERO(&readfds);              // Clear the set
FD_SET(fd, &readfds);           // Add fd to set
FD_ISSET(fd, &readfds);         // Check if fd in set (after select)
FD_CLR(fd, &readfds);           // Remove fd from set
```

**struct timeval:**
```c
struct timeval {
    time_t      tv_sec;     // seconds
    suseconds_t tv_usec;    // microseconds
};
```

**Return values:**
- `> 0`: Number of ready file descriptors
- `0`: Timeout expired, no FDs ready
- `-1`: Error (check errno)

**Important:** select() modifies fd_set to show which FDs are ready!
Must reinitialize fd_set and timeval on each iteration.

### 4. C Programming Patterns

**Opaque Types Pattern:**
- Common in Unix APIs for encapsulation
- Type is declared but internals hidden
- Macros/functions provide the interface
- Examples: fd_set, DIR*, sigset_t

**Finding Type Definitions:**
1. Man pages: `man 2 select`
2. Header files: `grep -r "struct timeval" /usr/include`
3. Online: man7.org, POSIX spec

**Don't chase typedefs down to basic types!** Trust the API contract.

---

## What We Implemented Today

### Problem: 1-Second Input Delay
**Original code:**
```c
while (running) {
    clear();
    draw_ui();
    refresh();

    ch = getch();  // Non-blocking (nodelay=TRUE)
    if (ch != ERR) {
        handle_input(ch);
    }

    sleep(1);  // ← Always sleeps! User input delayed up to 1 second
}
```

### Solution: select() with Timeout

**Implementation pattern:**
```c
fd_set readfds;
struct timeval timeout;

while (running) {
    // Reinitialize each iteration (select modifies them!)
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    // Draw UI
    clear();
    draw_header();
    draw_footer();
    refresh();

    // Wait for input OR timeout
    int result = select(1, &readfds, NULL, NULL, &timeout);

    if (result < 0) {
        // Error (will handle EINTR for signals next session)
    } else if (result > 0) {
        // Input ready - read immediately (won't block!)
        ch = getch();
        handle_input(ch);
    }
    // If result == 0: timeout, just loop and redraw
}
```

**Key changes:**
1. Added `#include <sys/select.h>` and `#include <sys/time.h>`
2. Removed `nodelay(stdscr, TRUE)` (conflicted with select)
3. Removed `sleep(REFRESH_INTERVAL)`
4. Moved `refresh()` before `select()` (render before waiting)
5. Initialize timeout inside loop (select may modify it)
6. Use FD_ZERO/FD_SET each iteration (select modifies fd_set)

**Result:** Program now responds instantly to keyboard input! ✓

---

## Next Session: Signal Handling (SIGWINCH)

### Current Problem
Window resize still has 1-second delay because we only check dimensions at loop start.

### What We'll Learn

**Signals (asynchronous notifications from kernel):**
- SIGINT (Ctrl+C), SIGTERM (kill), SIGWINCH (window resize), etc.
- Signal handlers: functions that run when signal arrives
- Interrupted system calls: select() returns -1 with errno=EINTR

**Signal delivery mechanisms:**
1. **During system call:** Immediate - kernel wakes blocked thread, returns EINTR
2. **During user code:** Via timer interrupt (~1-10ms delay)
3. Timer interrupts enable preemptive multitasking

**Implementation plan:**
```c
volatile sig_atomic_t resize_pending = 0;

void handle_sigwinch(int sig) {
    resize_pending = 1;  // Just set flag, keep handler minimal!
}

int main() {
    signal(SIGWINCH, handle_sigwinch);  // Install handler

    while (running) {
        if (resize_pending) {
            resize_pending = 0;
            // Handle resize
        }

        // ... normal loop ...

        result = select(...);
        if (result < 0) {
            if (errno == EINTR) {
                continue;  // Signal interrupted, check flag at top
            }
            // Real error
        }
    }
}
```

**Key concepts to implement:**
- Install signal handler with signal() or sigaction()
- Use volatile sig_atomic_t for flag (thread-safe)
- Keep handler minimal (just set flag!)
- Check errno for EINTR after select()
- Handle resize in main loop, not in handler

---

## Resources for Learning

**Man pages (essential for interviews!):**
- `man 2 select` - select system call
- `man 7 signal` - signal overview
- `man 2 sigaction` - install signal handlers
- Section numbers: 2=syscalls, 3=library functions, 5=file formats

**Finding struct definitions:**
```bash
man 2 select              # Shows struct timeval definition
grep -A 3 "struct timeval" /usr/include/sys/time.h
```

**Online resources:**
- https://man7.org/linux/man-pages/
- https://pubs.opengroup.org/onlinepubs/9699919799/ (POSIX)

---

## Interview Prep Takeaways

1. **Blocking vs non-blocking I/O** - understand the difference and use cases
2. **select/epoll** - how to multiplex I/O without threads
3. **C10K problem** - why async I/O matters for servers
4. **Signals** - asynchronous notifications, signal handlers, interrupted syscalls
5. **errno and EINTR** - distinguishing real errors from interruptions
6. **Opaque types** - C's encapsulation pattern
7. **Man pages** - how to research APIs during interviews

**Key interview insight:** Understanding WHY designs are the way they are (historical context, scalability, tradeoffs) is as important as knowing HOW they work.

---

## Code Status

**Branch:** `make_ui_interactive`
**Compiles:** ✓ Yes
**Runs:** ✓ Yes
**Input responsive:** ✓ Fixed!
**Window resize responsive:** ✗ Next session

**Current TODOs in code:**
- Line 151: Handle select() errors (add errno check for EINTR)
- Line 140: Add process listing functionality (future)
- Line 94: Implement help dialog (future)

---

## Build & Test

```bash
make clean && make
./processexplorer
```

**Test cases passed:**
- ✓ Press 'q' - exits immediately (no delay!)
- ✓ Wait without input - refreshes every 1 second
- ✓ Press 'r' - responds instantly
- ✗ Resize window - still has 1 second delay (to fix next session)

---

## Next Steps

1. Implement SIGWINCH signal handler for window resize
2. Add errno checking for EINTR in select() error handling
3. Begin reading /proc filesystem for process data
4. Implement scrollable process list with ncurses
5. Add sorting and filtering capabilities

**Focus for next session:** Complete signal handling, then move to /proc reading.
