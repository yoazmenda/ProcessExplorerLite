# Session Notes - Process Listing Feature (WIP)

**Date:** 2025-11-19
**Branch:** `feature/process-listing`
**Status:** Work in Progress

## What We Accomplished Today

### 1. Learned About `/proc` Filesystem
- Explored how Linux exposes process info through virtual filesystem
- Used `strace` to understand `opendir`/`readdir` → `openat`/`getdents64` mapping
- Learned about directory streams and lazy evaluation pattern
- Understood opaque pointers (`DIR*`) as C encapsulation pattern

### 2. Started Basic Process Listing
- Added `opendir("/proc")` to open directory
- Implemented `readdir()` loop to iterate entries
- Created `is_numeric()` helper to filter PID directories
- Started displaying PIDs in `draw_content()`

## Current Code State

**What Works:**
- Opens `/proc` directory successfully
- Reads directory entries in a loop
- Attempts to filter numeric PIDs

**Known Bugs (Need to Fix):**

### Bug 1: `is_numeric()` function issues
```c
// WRONG:
while (str != NULL) {  // ❌ str pointer never becomes NULL

// CORRECT:
while (*str != '\0') {  // ✓ Check character, not pointer
```

### Bug 2: Missing return statement
```c
// Missing at end of is_numeric():
return true;  // All characters were digits
```

### Bug 3: Missing include
```c
// Add at top:
#include <stdbool.h>  // For bool type
```

### Bug 4: No cleanup
```c
// After the while loop, add:
closedir(dir_stream);  // Prevent fd leak
```

### Bug 5: Wrong increment
```c
// Currently increments for ALL entries:
i++;  // Wrong place

// Should only increment for displayed PIDs:
if (is_numeric(dirent_ptr->d_name)) {
    mvprintw(i, 0, dirent_ptr->d_name);
    i++;  // Move here
}
```

## Next Steps (In Order)

1. **Fix `is_numeric()` bugs** (see above)
2. **Add `closedir()`** to prevent resource leak
3. **Fix `i++` placement** to only count displayed items
4. **Test basic PID listing** - compile and run
5. **Add process names** - read from `/proc/[pid]/comm`
6. **Display PID + Name** in format: "PID: 1234  Name: systemd"
7. **Test final version**
8. **Merge to main**

## Key Concepts Learned

- **Opaque Pointers:** `DIR*`, `FILE*` - handles you don't look inside
- **Streaming/Lazy Evaluation:** `readdir()` returns entries one-at-a-time
- **Virtual Filesystems:** `/proc` isn't real files, kernel generates on-demand
- **System Calls vs Library:** `opendir` → `openat`, `readdir` → `getdents64`
- **Separation of Concerns:** Discussed data collection vs rendering (decided to keep inline for v1)

## How to Resume

1. Checkout the branch:
   ```bash
   git checkout feature/process-listing
   ```

2. Review the bugs listed above

3. Fix them one-by-one (start with `is_numeric()`)

4. Compile and test:
   ```bash
   make clean && make
   ./processexplorer
   ```

5. Once PIDs display correctly, add process name reading

## Resources

- `man 3 opendir` - Opening directories
- `man 3 readdir` - Reading directory entries
- `man 3 closedir` - Closing directories
- `man 5 proc` - The /proc filesystem documentation

## Questions to Think About for Next Time

- How would we read the process name from `/proc/[pid]/comm`?
- What if we want to show process state (sleeping, running)?
- How do we handle the case where processes appear/disappear between reads?
- Should we limit how many processes we display (screen height)?
