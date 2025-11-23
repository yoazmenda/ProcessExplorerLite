#include "task_data.h"
#include <stdio.h>
#include <string.h>

/* ========== Task Data Collection ========== */

/*
 * TODO: Replace this mock implementation with actual /proc parsing
 *
 * To implement real data collection:
 * 1. Iterate through /proc/[pid]/task/[tid] directories
 * 2. Read /proc/[pid]/task/[tid]/stat or /proc/[pid]/task/[tid]/status
 * 3. Parse the data to extract PID, TID, command name, and state
 * 4. Populate the TaskInfo structs
 */
int collect_task_data(TaskInfo *tasks, int max_tasks) {
    const char *mock_commands[] = {
        "systemd", "kthreadd", "bash", "vim", "firefox",
        "chrome", "docker", "nginx", "postgres", "python3",
        "gcc", "make", "ssh", "sshd", "cron",
        "dbus-daemon", "NetworkManager", "pulseaudio", "Xorg", "gnome-shell"
    };
    const char states[] = {'R', 'S', 'S', 'S', 'D', 'S', 'S', 'S', 'S', 'S'};
    int num_commands = sizeof(mock_commands) / sizeof(mock_commands[0]);
    int count = 0;

    /* Generate mock tasks */
    for (int i = 0; i < 50 && count < max_tasks; i++) {
        int pid = 100 + i * 10;
        int num_threads = 1 + (i % 4);  /* 1-4 threads per process */

        for (int t = 0; t < num_threads && count < max_tasks; t++) {
            tasks[count].pid = pid;
            tasks[count].tid = pid + t;
            snprintf(tasks[count].command, sizeof(tasks[count].command),
                    "%s", mock_commands[i % num_commands]);
            tasks[count].state = states[count % 10];
            count++;
        }
    }

    return count;
}

const char* get_state_string(char state) {
    switch(state) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        default: return "Unknown";
    }
}
