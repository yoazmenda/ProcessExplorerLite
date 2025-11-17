/*
 * ProcessExplorerLite - Interactive Linux Process Explorer
 * A simple TUI application similar to 'top' for monitoring system processes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

/* Function prototypes */
void init_ui(void);
void cleanup_ui(void);
void draw_header(void);
void draw_footer(void);
void handle_input(int ch);

/* Global state */
int running = 1;

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
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    }
}

/*
 * Clean up and restore terminal to normal mode
 */
void cleanup_ui(void) {
    endwin();  /* End ncurses mode */
}

/*
 * Draw the header section with title and system info
 */
void draw_header(void) {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

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

    attron(COLOR_PAIR(2));
    mvprintw(max_y - 1, 0, "Keys: [q]uit | [r]efresh | [h]elp");
    attroff(COLOR_PAIR(2));
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

    /* Initialize UI */
    init_ui();

    fd_set readfds;
    struct timeval timeout; 
    int select_result; 
   
    /* Main loop */
    while (running) {
	timeout.tv_sec = 1; // one second
	timeout.tv_usec = 0; // zero microseconds 
	
	/* Clear the file descriptor set */
	FD_ZERO(&readfds);

	/* Insert our only file descriptor of interest: STDIN */
	FD_SET(STDIN_FILENO, &readfds);

	/* Clear screen */
        clear();

        /* Get terminal dimensions */
        getmaxyx(stdscr, max_y, max_x);

        /* Draw UI components */
        draw_header();
        draw_footer();

        /* Draw placeholder content */
        attron(COLOR_PAIR(3));
        mvprintw(3, 2, "Welcome to ProcessExplorerLite!");
        attroff(COLOR_PAIR(3));

        mvprintw(5, 2, "This is a boilerplate TUI application.");
        mvprintw(6, 2, "TODO: Add process listing functionality here");
        mvprintw(8, 2, "Terminal size: %d rows x %d cols", max_y, max_x);

        
	refresh(); 
	/* Handle input (non-blocking) */
	select_result = select(1, &readfds,
                  NULL,
		  NULL,                 
                  &timeout);
	if (select_result < 0) {
	    // TODO handle error when reading from SDTIN
	} else if (select_result > 0) {
	    ch = getch();
	    handle_input(ch);
	}
    }
    /* Clean up */
    cleanup_ui();

    printf("Thank you for using ProcessExplorerLite!\n");
    return 0;
}
