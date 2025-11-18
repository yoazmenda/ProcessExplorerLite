/*
 * ProcessExplorerLite - Interactive Linux Process Explorer
 * A simple TUI application similar to 'top' for monitoring system processes
 */

/* Feature test macro for POSIX.1-2008 (needed for sigaction, etc.) */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>

/* Function prototypes */
void init_ui(void);
void cleanup_ui(void);
void draw_header(void);
void draw_footer(void);
void draw_debug_panel(void);
void handle_input(int ch);
void handle_resize(void);

/* Global state */
int running = 1;
int debug_mode = 0;  /* Toggle with 'd' key */

/*
 * Signal handling: volatile sig_atomic_t for async-signal-safety
 * The signal handler ONLY sets flags - all actual work happens in main loop
 */
volatile sig_atomic_t resize_pending = 0;

/* Debug statistics for learning and troubleshooting */
static int resize_count = 0;
static int select_timeout_count = 0;
static int select_input_count = 0;
static int select_interrupt_count = 0;
static int last_errno = 0;

/*
 * Signal handler for SIGWINCH (window resize)
 *
 * CRITICAL: This runs in signal context - must be async-signal-safe!
 * We can ONLY:
 * - Set volatile sig_atomic_t variables
 * - Return
 *
 * We CANNOT:
 * - Call most functions (printf, malloc, ncurses functions, etc.)
 * - Access non-volatile variables
 * - Do complex logic
 */
void handle_sigwinch(int sig) {
    (void)sig;  /* Unused parameter */
    resize_pending = 1;
}

/*
 * Initialize the ncurses UI
 */
void init_ui(void) {
    initscr();              /* Start ncurses mode */
    cbreak();               /* Disable line buffering */
    noecho();               /* Don't echo typed characters */
    keypad(stdscr, TRUE);   /* Enable function keys */
    curs_set(0);            /* Hide cursor */

    /* Enable colors if terminal supports them */
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);    /* Header */
        init_pair(2, COLOR_GREEN, COLOR_BLACK);   /* Footer */
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);  /* Content */
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK); /* Debug */
    }
}

/*
 * Clean up and restore terminal to normal mode
 */
void cleanup_ui(void) {
    endwin();  /* End ncurses mode */
}

/*
 * Handle window resize - called when SIGWINCH is caught
 *
 * DESIGN RATIONALE:
 * When the terminal is resized, ncurses caches the old dimensions and doesn't
 * automatically know about the change. We must force ncurses to re-query.
 *
 * THE RELIABLE PATTERN: endwin() + refresh()
 *
 * Why this works:
 * 1. endwin() - Ends ncurses mode, causing it to FORGET cached dimensions
 * 2. refresh() - Restarts ncurses, which triggers a re-query via ioctl(TIOCGWINSZ)
 * 3. Now getmaxyx() will return the NEW dimensions
 *
 * Why NOT just resizeterm()?
 * - resizeterm(0, 0) doesn't work reliably on all systems/terminals
 * - The endwin()/refresh() pattern is the most portable and documented approach
 *
 * Note: We do NOT call clear() here to avoid double-clearing issues.
 * The main loop handles all clearing and redrawing.
 */
void handle_resize(void) {
    resize_count++;

    /* Force ncurses to forget cached dimensions and re-query the terminal */
    endwin();    /* End curses mode - ncurses forgets cached dimensions */
    refresh();   /* Restart curses mode - forces ioctl() to query real size */

    /* Main loop will now get updated dimensions from getmaxyx() */
}

/*
 * Draw the header section with title and system info
 */
void draw_header(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_y;  /* Unused */

    /* Title bar */
    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, "ProcessExplorerLite");
    mvprintw(0, max_x - 20, "Press 'q' to quit");
    attroff(COLOR_PAIR(1) | A_BOLD);

    /* Separator line */
    mvhline(1, 0, '-', max_x);
}

/*
 * Draw the footer with help text
 */
void draw_footer(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_x;  /* Unused */

    attron(COLOR_PAIR(2));
    mvprintw(max_y - 1, 0, "Keys: [q]uit | [r]efresh | [d]ebug | [h]elp");
    attroff(COLOR_PAIR(2));
}

/*
 * Draw debug panel showing internal state
 *
 * PURPOSE: Educational tool for understanding signal handling and I/O multiplexing
 * Shows real-time statistics about:
 * - Signal delivery (SIGWINCH)
 * - select() behavior (timeouts, inputs, interrupts)
 * - Error states (errno values)
 *
 * This helps visualize what's happening "under the hood" during development.
 */
void draw_debug_panel(void) {
    if (!debug_mode) {
        return;  /* Debug panel is hidden */
    }

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    /* Calculate debug panel position (bottom section, above footer) */
    int panel_height = 8;
    int panel_top = max_y - panel_height - 1;  /* -1 for footer */

    /* Draw separator and title */
    attron(COLOR_PAIR(4) | A_BOLD);
    mvhline(panel_top - 1, 0, '=', max_x);
    mvprintw(panel_top - 1, 2, "[ DEBUG PANEL ]");
    attroff(COLOR_PAIR(4) | A_BOLD);

    /* Display signal statistics */
    attron(COLOR_PAIR(4));
    mvprintw(panel_top,     2, "Signal Statistics:");
    mvprintw(panel_top + 1, 4, "SIGWINCH received: %d times", resize_count);
    mvprintw(panel_top + 2, 4, "resize_pending flag: %d", resize_pending);

    /* Display select() statistics */
    mvprintw(panel_top + 3, 2, "select() Statistics:");
    mvprintw(panel_top + 4, 4, "Timeouts: %d | Input events: %d | Interrupts (EINTR): %d",
             select_timeout_count, select_input_count, select_interrupt_count);

    /* Display error state */
    mvprintw(panel_top + 5, 2, "Last Error:");
    mvprintw(panel_top + 6, 4, "errno = %d (%s)",
             last_errno, last_errno == EINTR ? "EINTR - Interrupted by signal" :
                        last_errno == 0 ? "No error" : "Other");
    attroff(COLOR_PAIR(4));
}

/*
 * Handle keyboard input
 */
void handle_input(int ch) {
    switch(ch) {
        case 'q':
        case 'Q':
            running = 0;
            break;
        case 'r':
        case 'R':
            /* Force refresh - handled in main loop */
            break;
        case 'd':
        case 'D':
            /* Toggle debug mode */
            debug_mode = !debug_mode;
            break;
        case 'h':
        case 'H':
            /* TODO: Show help dialog */
            break;
    }
}

/*
 * Main application loop
 */
int main(void) {
    int ch;
    int max_y, max_x;
    struct sigaction sa;

    /*
     * CRITICAL: Register signal handler BEFORE initializing UI
     * Use sigaction() instead of signal() for reliable, persistent handler
     * signal() has inconsistent behavior - may reset after first call (old SysV)
     */
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Automatically restart interrupted system calls */
    sigaction(SIGWINCH, &sa, NULL);

    /* Initialize UI */
    init_ui();

    /* I/O multiplexing variables */
    fd_set readfds;
    struct timeval timeout;
    int select_result;

    /*
     * MAIN EVENT LOOP DESIGN:
     *
     * 1. Check signal flags (resize_pending) - "safe place" to handle signals
     * 2. Setup select() parameters (must reinit each iteration!)
     * 3. Draw UI with current state
     * 4. Wait for input OR timeout using select()
     * 5. Handle select() results:
     *    - Error: Check if EINTR (signal interrupt) or real error
     *    - Input available: Read and process
     *    - Timeout: Continue loop (auto-refresh)
     */
    while (running) {
        /*
         * STEP 1: Check signal flags at top of loop
         * This is our "safe place" - we have full control, can call any function
         */
        if (resize_pending) {
            resize_pending = 0;  /* Clear flag */
            handle_resize();     /* Update ncurses dimensions */
        }

        /* Get current terminal dimensions (may have just been updated!) */
        getmaxyx(stdscr, max_y, max_x);

        /*
         * STEP 2: Setup select() parameters
         * IMPORTANT: Must reinitialize timeout each iteration!
         * select() modifies the timeout struct on some systems
         */
        timeout.tv_sec = 1;      /* 1 second timeout */
        timeout.tv_usec = 0;     /* 0 microseconds */

        FD_ZERO(&readfds);                  /* Clear file descriptor set */
        FD_SET(STDIN_FILENO, &readfds);     /* Monitor stdin for input */

        /*
         * STEP 3: Draw UI
         * We draw AFTER checking signals and BEFORE select()
         * This ensures we always display the current state
         */
        clear();
        draw_header();
        draw_footer();

        /* Draw main content */
        attron(COLOR_PAIR(3));
        mvprintw(3, 2, "Welcome to ProcessExplorerLite!");
        attroff(COLOR_PAIR(3));

        mvprintw(5, 2, "This is a learning project for systems programming.");
        mvprintw(6, 2, "TODO: Add process listing functionality here");
        mvprintw(8, 2, "Terminal size: %d rows x %d cols", max_y, max_x);
        mvprintw(9, 2, "Debug mode: %s (toggle with 'd')", debug_mode ? "ON" : "OFF");

        /* Draw debug panel if enabled */
        draw_debug_panel();

        refresh();

        /*
         * STEP 4: Wait for input OR timeout using select()
         * This is where we're most likely to be interrupted by signals (SIGWINCH)
         */
        select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        /*
         * STEP 5: Handle select() results
         */
        if (select_result < 0) {
            /* Error occurred */
            last_errno = errno;

            if (errno == EINTR) {
                /*
                 * Interrupted by signal - NOT a real error!
                 * This is EXPECTED when a signal arrives during select()
                 * Just continue back to top of loop to check signal flags
                 */
                select_interrupt_count++;
                continue;
            }

            /* Real error - something is seriously wrong */
            perror("select");
            break;

        } else if (select_result > 0) {
            /* Input available on stdin */
            select_input_count++;
            last_errno = 0;
            ch = getch();
            handle_input(ch);

        } else {
            /* Timeout - no input for 1 second */
            select_timeout_count++;
            last_errno = 0;
            /* Just loop and redraw (auto-refresh every 1 second) */
        }
    }

    /* Clean up */
    cleanup_ui();

    /* Print session statistics */
    printf("Thank you for using ProcessExplorerLite!\n");
    printf("Session statistics:\n");
    printf("  - Window resizes: %d\n", resize_count);
    printf("  - select() timeouts: %d\n", select_timeout_count);
    printf("  - User inputs: %d\n", select_input_count);
    printf("  - Signal interrupts: %d\n", select_interrupt_count);

    return 0;
}
