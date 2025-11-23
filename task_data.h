#ifndef TASK_DATA_H
#define TASK_DATA_H

/* ========== Task Data Structures ========== */

typedef struct {
    int pid;
    int tid;
    char command[32];
    char state;  /* 'R' = Running, 'S' = Sleeping, 'D' = Disk sleep, 'Z' = Zombie, 'T' = Stopped */
} TaskInfo;

#define MAX_TASKS 1000

/* ========== Task Data Functions ========== */

/* Collect task data and populate the tasks array
 * TODO: Replace mock implementation with actual /proc parsing
 * Returns: number of tasks collected
 */
int collect_task_data(TaskInfo *tasks, int max_tasks);

/* Get human-readable string for task state */
const char* get_state_string(char state);

#endif /* TASK_DATA_H */
