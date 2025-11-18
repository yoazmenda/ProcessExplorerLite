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
void draw_content(void);
void draw_debug_panel(void);
void handle_input(int ch);
void handle_resize(void);

/* Global state */
int running = 1;
int debug_mode = 0;

/* Signal flag - volatile sig_atomic_t for async-signal-safety */
volatile sig_atomic_t resize_pending = 0;

/* Debug statistics */
static int resize_count = 0;
static int select_timeout_count = 0;
static int select_input_count = 0;
static int select_interrupt_count = 0;
static int last_errno = 0;

/*
 * Signal handler for SIGWINCH (window resize)
 * IMPORTANT: Runs in signal context - must be async-signal-safe!
 * Only sets a flag; actual work happens in main loop.
 */
void handle_sigwinch(int sig) {
    (void)sig;
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
 * Handle window resize
 *
 * The endwin()/refresh() pattern forces ncurses to re-query terminal dimensions:
 * - endwin() ends ncurses mode, clearing cached dimensions
 * - refresh() restarts and calls ioctl(TIOCGWINSZ) to get actual size
 * - Main loop's getmaxyx() will then return updated dimensions
 */
void handle_resize(void) {
    resize_count++;
    endwin();
    refresh();
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
 * Draw main content area
 */
void draw_content(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    attron(COLOR_PAIR(3));
    mvprintw(3, 2, "Welcome to ProcessExplorerLite!");
    attroff(COLOR_PAIR(3));

    mvprintw(5, 2, "This is a learning project for systems programming.");
    mvprintw(6, 2, "TODO: Add process listing functionality here");
    mvprintw(8, 2, "Terminal size: %d rows x %d cols", max_y, max_x);
    mvprintw(9, 2, "Debug mode: %s (toggle with 'd')", debug_mode ? "ON" : "OFF");
}

/*
 * Draw debug panel with signal/select() statistics
 * Educational tool for visualizing system call behavior in real-time
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
    int max_y, max_x;  /* Queried after resize - needed for ncurses state sync */
    struct sigaction sa;

    /*
     * Register signal handler BEFORE UI init
     * Use sigaction() not signal() - signal() may reset after first call (SysV)
     */
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  /* Auto-restart interrupted syscalls */
    sigaction(SIGWINCH, &sa, NULL);

    /* Initialize UI */
    init_ui();

    /* I/O multiplexing variables */
    fd_set readfds;
    struct timeval timeout;
    int select_result;

    /*
     * EVENT LOOP: Handle signals → Draw UI → Wait for input → Repeat
     */
    while (running) {
        /* Handle pending signals (safe context - can call any function) */
        if (resize_pending) {
            resize_pending = 0;
            handle_resize();
        }

        /* Query dimensions - forces ncurses state sync after resize */
        getmaxyx(stdscr, max_y, max_x);
        (void)max_y; (void)max_x;

        /* Setup select() - must reinit timeout each iteration! */
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        /* Draw UI */
        clear();
        draw_header();
        draw_content();
        draw_debug_panel();
        draw_footer();
        refresh();

        /* Wait for input or timeout (signals interrupt here) */
        select_result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

        /* Handle results */
        if (select_result < 0) {
            last_errno = errno;
            if (errno == EINTR) {
                /* Signal interrupted - expected, not an error */
                select_interrupt_count++;
                continue;
            }
            perror("select");
            break;
        } else if (select_result > 0) {
            /* Input available */
            select_input_count++;
            last_errno = 0;
            ch = getch();
            handle_input(ch);
        } else {
            /* Timeout - auto-refresh */
            select_timeout_count++;
            last_errno = 0;
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
