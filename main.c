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

/* ========== Global State ========== */

int running = 1;
int debug_mode = 0;
volatile sig_atomic_t resize_pending = 0;

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
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK);
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
    mvprintw(max_y - 1, 0, "Keys: [q]uit | [d]ebug | [h]elp");
    attroff(COLOR_PAIR(2));
}

bool is_numeric(char* str) {
    if (str == NULL) { // definitly not a number
        return false;
    }
    if (str[0] == '\0') { // empty string isn't a number
        return false;
    }

    while (str != NULL) {
        char current = str[0];
        int is_digit_result = isdigit((unsigned char)current);
        if (is_digit_result == false) {
	    return false;
	}
  	str++;
    }
}


void draw_content(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    DIR *dir_stream = opendir("/proc");
    struct dirent *dirent_ptr;
    int i = 3;
    last_errno = errno;
    while ((dirent_ptr = readdir(dir_stream)) != NULL) {
	if (is_numeric(dirent_ptr->d_name)) {	 
	    // it's actually a directory like /prod/D where D is a pid
            mvprintw(i, 0, dirent_ptr->d_name);
	}
        i++;
    }
    if (dirent_ptr == NULL && errno != last_errno) {
	// not only NULL was returned but errno has been changed. according to docs, it's an error
	perror("readdir");
        running = 0;
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
    switch(ch) {
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

int wait_for_input(fd_set *readfds, struct timeval *timeout) {
    timeout->tv_sec = 1;
    timeout->tv_usec = 0;
    FD_ZERO(readfds);
    FD_SET(STDIN_FILENO, readfds);

    return select(STDIN_FILENO + 1, readfds, NULL, NULL, timeout);
}

void process_select_result(int result) {
    if (result < 0) {
        last_errno = errno;
        if (errno == EINTR) {
            select_interrupt_count++;
            return;
        }
        perror("select");
        running = 0;
    } else if (result > 0) {
        select_input_count++;
        last_errno = 0;
        handle_input(getch());
    } else {
        select_timeout_count++;
        last_errno = 0;
    }
}

/* ========== Main ========== */

int main(void) {
    fd_set readfds;
    struct timeval timeout;

    signal(SIGWINCH, handle_sigwinch);
    init_ui();

    while (running) {
        if (resize_pending) {
            resize_pending = 0;
            handle_resize();
        }

        draw_ui();
        process_select_result(wait_for_input(&readfds, &timeout));
    }

    cleanup_ui();
    return 0;
}
