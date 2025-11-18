# ProcessExplorerLite - Session 2

## Session Date: 2025-11-18

### Session Goals
- Understand signal handling in Linux (SIGWINCH for window resize)
- Learn about async-signal-safety and reentrancy
- Understand how signals interact with system calls (EINTR)
- Prepare to implement responsive window resizing

---

## Session Status: 🔄 Theory & Discussion (Implementation Pending)

**What we covered:** Deep dive into signal handling concepts
**What's next:** Implement SIGWINCH handler + EINTR checking

---

## Key Concepts Explored

### 1. How Signals Are Delivered

**Definition:** Signals are "software interrupts" - asynchronous notifications from kernel to process

**Signal Sources:**
- **Kernel-generated:** SIGWINCH (window resize), SIGSEGV (segfault), SIGALRM (timer)
- **Process-generated:** via `kill()` syscall (requires proper permissions)

**Two Cases for Signal Delivery:**

**Case 1: Process is executing a system call**
```
1. Process blocked in syscall (e.g., select())
2. Signal arrives → kernel interrupts the syscall
3. Signal handler executes (if registered)
4. Syscall returns -1 with errno = EINTR
5. Main code continues after the syscall
```

**Case 2: Process is running user code (not in syscall)**
```
1. Timer interrupt fires (~1-10ms intervals)
2. Kernel checks for pending signals
3. Signal handler executes
4. Returns to user code
```

**Purpose of timer interrupts:**
- **Preemptive multitasking:** Let scheduler run other processes (fairness)
- **Signal delivery:** Can't let processes avoid signals by not making syscalls

`★ Key Insight: Timer interrupts enable preemptive multitasking. Without them, a process doing while(1); could monopolize CPU!`

---

### 2. Signal Handler Execution Sequence

**Critical understanding: Handler runs FIRST, then syscall returns!**

```
Timeline when SIGWINCH interrupts select():

1. select() blocks, waiting for input
2. User resizes terminal
3. Kernel receives notification → marks SIGWINCH as pending
4. ⚡ Signal handler executes → sets resize_pending = 1
5. Handler returns
6. ✅ select() returns -1, errno = EINTR
7. Main code continues execution
```

**Why this matters:**
- Flag is already set by the time you check errno
- Handler is synchronous - runs to completion before syscall returns
- Can safely check flag immediately after interrupted syscall

---

### 3. Async-Signal-Safety (Critical Interview Topic!)

**Question:** Why not handle resize directly in signal handler?

**Answer:** Reentrancy and async-signal-safety!

**The Danger:**
```c
// Main code
printf("Processing...");  // ← Inside printf(), has locked buffers

// SIGWINCH arrives!
void handle_sigwinch(int sig) {
    printf("Resized!\n");  // ⚠️ Tries to lock same buffers
                           // → DEADLOCK or heap corruption!
}
```

**The Problem: Reentrancy**
- Main code calls function → locks mutex/modifies global state
- Signal interrupts → handler calls same function
- Corruption or deadlock!

**Async-signal-safe functions:** Can be safely called from signal handlers
- ✅ Safe: `write()`, `_exit()`, `signal()`, `sigaction()`
- ❌ Unsafe: `printf()`, `malloc()`, `free()`, **ALL ncurses functions**

**Why ncurses is unsafe:**
- Maintains complex internal state
- Uses malloc/free
- Not designed for reentrancy

**Best Practice:** Signal handlers should ONLY set a flag!
```c
volatile sig_atomic_t resize_pending = 0;

void handle_sigwinch(int sig) {
    resize_pending = 1;  // ← Minimal, safe
}
```

**Why `volatile sig_atomic_t`?**
- `sig_atomic_t`: Guaranteed atomic read/write (no race conditions)
- `volatile`: Prevents compiler optimization (ensures main loop sees changes)

---

### 4. Signal Handler Runs to Completion

**Understanding "runs to completion":**
- By default, signal handler is NOT preempted
- Runs until it returns (no scheduler interruption)
- Same signal type is blocked during handler execution

**What if handler has infinite loop?**
```c
void bad_handler(int sig) {
    while(1);  // ⚠️ Process hangs!
}
```
- Your process freezes (not whole system)
- Kernel can still kill with SIGKILL
- Other processes unaffected
- **This is why handlers must be minimal!**

---

### 5. Implementation Pattern Discussion

**Two approaches discussed:**

**Minimal Approach (works for our simple case):**
```c
result = select(1, &readfds, NULL, NULL, &timeout);

if (result < 0) {
    if (errno == EINTR) {
        continue;  // Next iteration calls getmaxyx() anyway
    }
    // Real error
}
```

**Pros:** Simple, works since we query dimensions every loop
**Cons:** Implicit, doesn't scale to multiple signals

**Full Pattern (best practice):**
```c
volatile sig_atomic_t resize_pending = 0;

void handle_sigwinch(int sig) {
    resize_pending = 1;
}

int main() {
    signal(SIGWINCH, handle_sigwinch);

    while (running) {
        if (resize_pending) {
            resize_pending = 0;
            // Explicit resize handling
            // Could reinit ncurses, clear screen, etc.
        }

        result = select(...);

        if (result < 0) {
            if (errno == EINTR) {
                continue;  // Signal interrupted, check flags at loop top
            }
            // Real error handling
        }
    }
}
```

**Pros:**
- Explicit and readable
- Scales to multiple signal types
- Interview-friendly (demonstrates understanding)
- Allows specific action on resize vs timeout

**Why you need BOTH handler and EINTR check:**
- **Handler:** Tells you WHAT happened (which signal)
- **EINTR check:** Tells you syscall was interrupted (not a real error)
- **errno alone can't tell you which signal** - just "interrupted by some signal"

---

### 6. Multiple Signals Example

```c
volatile sig_atomic_t resize_pending = 0;
volatile sig_atomic_t pause_pending = 0;
volatile sig_atomic_t shutdown_pending = 0;

void handle_sigwinch(int sig) { resize_pending = 1; }
void handle_sigusr1(int sig) { pause_pending = 1; }
void handle_sigterm(int sig) { shutdown_pending = 1; }

int main() {
    signal(SIGWINCH, handle_sigwinch);
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGTERM, handle_sigterm);

    while (running) {
        if (resize_pending) { /* handle resize */ }
        if (pause_pending) { /* handle pause */ }
        if (shutdown_pending) { running = 0; }

        result = select(...);

        if (result < 0 && errno == EINTR) {
            continue;  // Check all flags at top of loop
        }
    }
}
```

**This shows why flags are essential:** EINTR doesn't tell you which signal arrived!

---

## Student's Learning Process (Excellent Analytical Thinking!)

### Questions Asked & Insights Gained:

1. **"When does select() actually return after a signal?"**
   - Discovered: Handler runs FIRST, then select() returns
   - Understood the synchronous nature of signal handlers

2. **"What happens if I call syscalls inside handler?"**
   - Initial theory: Infinite loop from pending signals
   - Actual answer: Reentrancy issues and async-signal-safety
   - Key insight: It's about corruption, not loops!

3. **"What if handler has infinite loop?"**
   - Correctly reasoned: Process would hang (not system)
   - Understood: Handlers run to completion without preemption

4. **"Isn't EINTR check enough? We query dimensions anyway!"**
   - EXCELLENT critical thinking!
   - Discovered: Minimal approach works, but full pattern is better practice
   - Understood trade-offs: simplicity vs. robustness

---

## What We Haven't Implemented Yet

**Still TODO (next session):**
- [ ] Add `#include <signal.h>` and `#include <errno.h>`
- [ ] Declare `volatile sig_atomic_t resize_pending = 0;`
- [ ] Implement `handle_sigwinch()` function
- [ ] Register handler with `signal(SIGWINCH, handle_sigwinch);`
- [ ] Add flag check in main loop
- [ ] Add EINTR error handling after select()
- [ ] Test window resize responsiveness

**Decision point for next session:**
Which approach to implement?
- Option A: Minimal (just EINTR check)
- Option B: Full pattern (handler + flag + EINTR)

Recommendation: Option B for interview prep and best practices

---

## Interview Prep Concepts Covered

### Core Topics:
1. ✅ **Signal delivery mechanisms** (during syscall vs user code)
2. ✅ **Timer interrupts** and preemptive multitasking
3. ✅ **Signal handler execution** (synchronous, runs to completion)
4. ✅ **Async-signal-safety** and reentrancy
5. ✅ **EINTR error handling** (interrupted system calls)
6. ✅ **volatile sig_atomic_t** (why and when)
7. ✅ **Signal handler best practices** (minimal, just set flags)

### Interview Questions You Can Now Answer:
- "How are signals delivered to a process?"
- "What is async-signal-safety and why does it matter?"
- "Why can't you call printf() in a signal handler?"
- "What does EINTR mean and how do you handle it?"
- "How do timer interrupts enable preemptive multitasking?"
- "Why should signal handlers be minimal?"

---

## Resources & Man Pages

**Essential reading:**
- `man 7 signal` - Signal overview
- `man 2 signal` - Install signal handler (simple version)
- `man 2 sigaction` - Install signal handler (advanced, with flags)
- `man 7 signal-safety` - Async-signal-safe functions list

**Key concepts to research:**
- SA_RESTART flag (auto-restart interrupted syscalls)
- sigprocmask() (blocking signals)
- Signal disposition (default actions)

---

## Next Session Plan

### Implementation Tasks:
1. Decide: Minimal or full pattern?
2. Add signal handling code to main.c
3. Test window resize responsiveness
4. Verify no more 1-second delay on resize!

### Stretch Goals:
- Handle other signals (SIGINT for graceful Ctrl+C exit?)
- Use sigaction() instead of signal() (more control)
- Begin /proc filesystem reading (if time permits)

---

## Code Status

**Branch:** `master`
**Compiles:** ✓ Yes
**Runs:** ✓ Yes
**Input responsive:** ✓ Yes (from Session 1)
**Window resize responsive:** ✗ Not yet (in progress)

**Current TODOs in code:**
- Line 151: Handle select() errors (will add errno check for EINTR)
- Line 140: Add process listing functionality (future)
- Line 94: Implement help dialog (future)

---

## Key Takeaways

`★ Insight: Signal handlers are like interrupts in hardware - they can fire at ANY time. This unpredictability is why async-signal-safety matters. You must assume your handler interrupted ANY line of code, including malloc() or printf() internals!`

**The Golden Rule:** Signal handlers should do the absolute minimum - typically just set a flag. All real work happens in the main loop where you have full control.

**Pattern to remember:**
```
Handler sets flag → Syscall interrupted → Returns EINTR →
Main loop checks flag → Takes appropriate action
```

---

## Session Outcome

**Technical Progress:** None (no code written yet)
**Learning Progress:** 🌟 Excellent! Deep understanding of signals, async-safety, and EINTR

The student demonstrated:
- Systematic analytical thinking
- Good questioning and self-correction
- Understanding of kernel/user mode boundaries
- Critical thinking about design tradeoffs

**Ready for implementation!** Next session will be hands-on coding.

---

**Session ended:** 2025-11-18
**Duration:** Theory & discussion
**Next:** Implement SIGWINCH handler
