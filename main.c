/*
 * ProcessExplorerLite - Interactive Linux Process Explorer
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>

#include "task_data.h"

/* ========== Global State ========== */

int running = 1;
int debug_mode = 0;
volatile sig_atomic_t resize_pending = 0;

/* Task list state */
TaskInfo tasks[MAX_TASKS];
int task_count = 0;
int selected_index = 0;  /* Currently selected row */
int scroll_offset = 0;   /* Top visible row */

/* Debug statistics */
static int resize_count = 0;
static int select_timeout_count = 0;
static int select_input_count = 0;
static int select_interrupt_count = 0;
static int last_errno = 0;

/* ========== Signal Handling ========== */

void handle_sigwinch(int sig) {
    (void)sig;
    signal(SIGWINCH, handle_sigwinch);  /* Re-register to handle SysV reset */
    resize_pending = 1;
}

void handle_resize(void) {
    resize_count++;
    endwin();
    refresh();
}

/* ========== UI Functions ========== */

void init_ui(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);    /* Header */
        init_pair(2, COLOR_GREEN, COLOR_BLACK);   /* Footer */
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);  /* Table header */
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK); /* Debug panel */
        init_pair(5, COLOR_BLACK, COLOR_WHITE);   /* Selected row */
        init_pair(6, COLOR_GREEN, COLOR_BLACK);   /* Running state */
        init_pair(7, COLOR_BLUE, COLOR_BLACK);    /* Sleeping state */
    }
}

void cleanup_ui(void) {
    endwin();
}

void draw_header(void) {
    int max_x;
    char time_str[64];

    max_x = getmaxx(stdscr);

    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

    attron(COLOR_PAIR(1) | A_BOLD);
    mvprintw(0, 0, "ProcessExplorerLite");
    mvprintw(0, max_x - strlen(time_str), "%s", time_str);
    attroff(COLOR_PAIR(1) | A_BOLD);
    mvhline(1, 0, '-', max_x);
}

void draw_footer(void) {
    int max_y;
    max_y = getmaxy(stdscr);

    attron(COLOR_PAIR(2));
    mvprintw(max_y - 1, 0, "Keys: [Up/Down]Navigate | [q]uit | [d]ebug | [h]elp");
    attroff(COLOR_PAIR(2));
}

int is_numeric(char* str) {
    if (*str == '\0') return 0;
    while (*str != '\0') {
        int is_digit_result = isdigit((unsigned char)*str);
        if (!is_digit_result) {
	    return 0;
	}
  	str++;
    }
    return 1;
}

/* ========== UI Helper Functions ========== */

int get_state_color(char state) {
    switch(state) {
        case 'R': return COLOR_PAIR(6);  /* Green for running */
        case 'S': return COLOR_PAIR(7);  /* Blue for sleeping */
        default: return 0;
    }
}

void draw_content(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    /* Calculate available space for task list */
    int header_lines = 2;  /* Title + separator */
    int footer_lines = 1;
    int debug_lines = debug_mode ? 9 : 0;
    int table_header_lines = 2;  /* Column headers + separator */

    int available_lines = max_y - header_lines - footer_lines - debug_lines - table_header_lines;
    int content_start_y = header_lines;

    /* Draw table header */
    attron(COLOR_PAIR(3) | A_BOLD);
    mvprintw(content_start_y, 2, "%-8s %-8s %-20s %-12s", "PID", "TID", "Command", "State");
    attroff(COLOR_PAIR(3) | A_BOLD);
    mvhline(content_start_y + 1, 0, '-', max_x);

    /* Draw task rows */
    int table_start_y = content_start_y + table_header_lines;

    for (int i = 0; i < available_lines && (scroll_offset + i) < task_count; i++) {
        int task_idx = scroll_offset + i;
        TaskInfo *task = &tasks[task_idx];
        int row_y = table_start_y + i;

        /* Highlight selected row */
        if (task_idx == selected_index) {
            attron(COLOR_PAIR(5) | A_BOLD);
            mvhline(row_y, 0, ' ', max_x);  /* Fill entire row with background */
        }

        /* Draw task info */
        mvprintw(row_y, 2, "%-8d %-8d %-20s", task->pid, task->tid, task->command);

        /* Draw state with color (only if not selected, to maintain readability) */
        if (task_idx == selected_index) {
            mvprintw(row_y, 2 + 8 + 1 + 8 + 1 + 20 + 1, "%-12s", get_state_string(task->state));
            attroff(COLOR_PAIR(5) | A_BOLD);
        } else {
            attron(get_state_color(task->state));
            mvprintw(row_y, 2 + 8 + 1 + 8 + 1 + 20 + 1, "%-12s", get_state_string(task->state));
            attroff(get_state_color(task->state));
        }
    }

    /* Draw scroll indicator if needed */
    if (task_count > available_lines) {
        int indicator_y = content_start_y + 3;
        attron(COLOR_PAIR(3));
        mvprintw(indicator_y, max_x - 15, "[%d/%d]", selected_index + 1, task_count);
        attroff(COLOR_PAIR(3));
    }
}

void draw_debug_panel(void) {
    if (!debug_mode) return;

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    int panel_height = 8;
    int panel_top = max_y - panel_height - 1;

    attron(COLOR_PAIR(4) | A_BOLD);
    mvhline(panel_top - 1, 0, '=', max_x);
    mvprintw(panel_top - 1, 2, "[ DEBUG PANEL ]");
    attroff(COLOR_PAIR(4) | A_BOLD);

    attron(COLOR_PAIR(4));
    mvprintw(panel_top,     2, "Signal Statistics:");
    mvprintw(panel_top + 1, 4, "SIGWINCH received: %d times", resize_count);
    mvprintw(panel_top + 2, 4, "resize_pending flag: %d", resize_pending);

    mvprintw(panel_top + 3, 2, "select() Statistics:");
    mvprintw(panel_top + 4, 4, "Timeouts: %d | Input events: %d | Interrupts (EINTR): %d",
             select_timeout_count, select_input_count, select_interrupt_count);

    mvprintw(panel_top + 5, 2, "Last Error:");
    mvprintw(panel_top + 6, 4, "errno = %d (%s)",
             last_errno, last_errno == EINTR ? "EINTR - Interrupted by signal" :
                        last_errno == 0 ? "No error" : "Other");
    attroff(COLOR_PAIR(4));
}

void draw_ui(void) {
    clear();
    draw_header();
    draw_content();
    draw_debug_panel();
    draw_footer();
    refresh();
}

/* ========== Input Handling ========== */

void handle_input(int ch) {
    int max_y;
    max_y = getmaxy(stdscr);

    /* Calculate visible lines for scrolling */
    int header_lines = 2;
    int footer_lines = 1;
    int debug_lines = debug_mode ? 9 : 0;
    int table_header_lines = 2;
    int available_lines = max_y - header_lines - footer_lines - debug_lines - table_header_lines;

    switch(ch) {
        case KEY_UP:
            if (selected_index > 0) {
                selected_index--;
                /* Scroll up if selection moves above visible area */
                if (selected_index < scroll_offset) {
                    scroll_offset = selected_index;
                }
            }
            break;

        case KEY_DOWN:
            if (selected_index < task_count - 1) {
                selected_index++;
                /* Scroll down if selection moves below visible area */
                if (selected_index >= scroll_offset + available_lines) {
                    scroll_offset = selected_index - available_lines + 1;
                }
            }
            break;

        case 'q':
        case 'Q':
            running = 0;
            break;

        case 'd':
        case 'D':
            debug_mode = !debug_mode;
            break;

        case 'h':
        case 'H':
            /* TODO: Show help dialog */
            break;
    }
}

/* ========== Main Event Loop ========== */

/*
 * Check for keyboard input with timeout
 * Uses select() to wait up to 1 second for input, allowing periodic UI refresh
 * Returns: 1 if input available, 0 if timeout, -1 on error
 */
int check_for_keyboard_input(void) {
    fd_set readfds;
    struct timeval timeout;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    return select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
}

int main(void) {
    signal(SIGWINCH, handle_sigwinch);
    init_ui();

    /* Collect task data */
    task_count = collect_task_data(tasks, MAX_TASKS);

    /* Main event loop: refresh UI every second and handle keyboard input */
    while (running) {
        /* Handle terminal resize */
        if (resize_pending) {
            resize_pending = 0;
            handle_resize();
        }

        /* Redraw the UI */
        draw_ui();

        /* Wait for keyboard input (with 1 second timeout for periodic refresh) */
        int input_status = check_for_keyboard_input();

        if (input_status < 0) {
            /* Error occurred */
            last_errno = errno;
            if (errno == EINTR) {
                /* Interrupted by signal (e.g., SIGWINCH) - just continue */
                select_interrupt_count++;
                continue;
            }
            /* Fatal error */
            perror("select");
            running = 0;

        } else if (input_status > 0) {
            /* Keyboard input is available - read and handle it */
            select_input_count++;
            last_errno = 0;
            int ch = getch();
            handle_input(ch);

        } else {
            /* Timeout - no input, just continue to refresh UI */
            select_timeout_count++;
            last_errno = 0;
        }
    }

    cleanup_ui();
    return 0;
}
