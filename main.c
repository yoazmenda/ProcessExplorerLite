/*
 * ProcessExplorerLite - Interactive Linux Process Explorer
 * A simple TUI application similar to 'top' for monitoring system processes
 */

/*
 * This tells the compiler to enable POSIX features (Unix standard functions).
 * Without this, functions like sigaction() won't be available in strict C99 mode.
 */
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
int wait_for_input(fd_set *readfds, struct timeval *timeout);

/* Application state */
int running = 1;          /* Set to 0 to quit the program */
int debug_mode = 0;       /* Toggle with 'd' key to show debug panel */

/*
 * Flag set by signal handler when terminal is resized.
 * "volatile sig_atomic_t" means it's safe to use in signal handlers.
 */
volatile sig_atomic_t resize_pending = 0;

/* Statistics for the debug panel */
static int resize_count = 0;              /* How many times terminal was resized */
static int select_timeout_count = 0;      /* How many 1-second timeouts occurred */
static int select_input_count = 0;        /* How many keypresses received */
static int select_interrupt_count = 0;    /* How many times signals interrupted us */
static int last_errno = 0;                /* Last error number from system calls */

/*
 * This function is called automatically when you resize the terminal window.
 * SIGWINCH = "SIGnal WINdow CHange" - it's a notification from the operating system.
 *
 * IMPORTANT: Signal handlers have strict rules!
 * We can ONLY set simple flags here. All the real work (updating the display)
 * happens in the main loop where it's safe to call ncurses functions.
 */
void handle_sigwinch(int sig) {
    (void)sig;  /* We don't need the signal number, so mark it unused */
    resize_pending = 1;  /* Set flag so main loop knows to handle resize */
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
 * Handle terminal resize
 *
 * When you resize your terminal window, ncurses (the library we use for drawing)
 * doesn't automatically know about it. We need to tell it explicitly.
 *
 * We do this by:
 * 1. endwin() - Temporarily turn off ncurses (makes it forget the old size)
 * 2. refresh() - Turn ncurses back on (it asks the OS for the new size)
 *
 * After this, ncurses knows the new terminal size and can draw correctly.
 */
void handle_resize(void) {
    resize_count++;  /* Track how many resizes for debug panel */
    endwin();        /* Turn off ncurses */
    refresh();       /* Turn it back on - it will ask OS for new size */
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
 * Draw the debug panel (press 'd' to toggle)
 * Shows what's happening behind the scenes - useful for learning!
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
 * Wait for user to press a key (with 1 second timeout)
 *
 * We use select() which is like saying "wait for input, but don't wait forever".
 * This lets the screen auto-refresh every second even if you don't press anything.
 *
 * Returns:
 *   1  = User pressed a key (input available)
 *   0  = Timeout (no key pressed in 1 second, just continue)
 *  -1  = Error (something went wrong, should exit program)
 */
int wait_for_input(fd_set *readfds, struct timeval *timeout) {
    int result = select(STDIN_FILENO + 1, readfds, NULL, NULL, timeout);

    if (result < 0) {
        last_errno = errno;
        if (errno == EINTR) {
            /*
             * EINTR = "Error INTerRupted" - a signal arrived while we were waiting.
             * This is normal when the terminal is resized! Not a real error.
             */
            select_interrupt_count++;
            return 0;  /* No input yet, continue loop */
        }
        /* Real error happened */
        perror("select");
        return -1;  /* Tell main loop to exit */
    } else if (result > 0) {
        /* User pressed a key! */
        select_input_count++;
        last_errno = 0;
        return 1;  /* Input is available */
    } else {
        /* Timeout - 1 second passed with no keypress */
        select_timeout_count++;
        last_errno = 0;
        return 0;  /* No input, loop will continue and redraw */
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
     * Step 1: Tell the OS to call handle_sigwinch() when terminal is resized
     *
     * We use sigaction() instead of signal() because signal() has a bug on some
     * systems where it stops working after being called once. sigaction() always works.
     */
    sa.sa_handler = handle_sigwinch;  /* Function to call on resize */
    sigemptyset(&sa.sa_mask);         /* Don't block other signals */
    sa.sa_flags = SA_RESTART;         /* Auto-restart system calls if interrupted */
    sigaction(SIGWINCH, &sa, NULL);   /* Register the handler with OS */

    /* Step 2: Initialize ncurses (the text UI library) */
    init_ui();

    /* Variables for waiting on user input */
    fd_set readfds;         /* File descriptor set (tells select() what to monitor) */
    struct timeval timeout; /* How long to wait */
    int select_result;      /* Result from wait_for_input() */

    /*
     * Step 3: Main loop - runs continuously until user presses 'q'
     *
     * The loop does 4 things:
     * 1. Check if terminal was resized, update if needed
     * 2. Draw the UI
     * 3. Wait for user input (with 1 second timeout for auto-refresh)
     * 4. If user pressed a key, handle it
     */
    while (running) {
        /* 1. Handle resize if it happened */
        if (resize_pending) {
            resize_pending = 0;  /* Clear the flag */
            handle_resize();     /* Tell ncurses about new terminal size */
        }

        /* Get current terminal size (needed after resize) */
        getmaxyx(stdscr, max_y, max_x);
        (void)max_y; (void)max_x;  /* Mark as used to avoid compiler warnings */

        /* 2. Draw everything */
        clear();            /* Clear old content */
        draw_header();      /* Draw top bar */
        draw_content();     /* Draw main content */
        draw_debug_panel(); /* Draw debug info (if enabled) */
        draw_footer();      /* Draw bottom bar */
        refresh();          /* Actually display everything */

        /* 3. Wait for user input */
        timeout.tv_sec = 1;               /* Wait up to 1 second */
        timeout.tv_usec = 0;              /* Plus 0 microseconds */
        FD_ZERO(&readfds);                /* Clear the monitor set */
        FD_SET(STDIN_FILENO, &readfds);   /* Monitor keyboard (stdin) */

        select_result = wait_for_input(&readfds, &timeout);

        /* 4. Handle user input */
        if (select_result < 0) {
            break;  /* Error occurred - exit program */
        } else if (select_result > 0) {
            ch = getch();       /* Read the key that was pressed */
            handle_input(ch);   /* Process it (might quit if 'q' was pressed) */
        }
        /* If select_result == 0, timeout occurred - loop continues and redraws */
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
