#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h>
#include "process_control.h"

void suspend_process(int pid) {
    if (kill(pid, SIGSTOP) != 0) {
        // Use stderr to avoid UI corruption; redirect to log file if needed
        fprintf(stderr, "Error: Failed to suspend process %d\n", pid);
    }
}

void resume_process(int pid) {
    if (kill(pid, SIGCONT) != 0) {
        fprintf(stderr, "Error: Failed to resume process %d\n", pid);
    }
}

void terminate_process(int pid) {
    // Attempt to kill process group first, then individual PID
    if (kill(-pid, SIGTERM) != 0) {
        if (kill(pid, SIGTERM) != 0) {
            fprintf(stderr, "Error: Failed to terminate process %d\n", pid);
        }
    }
}

void force_kill(int pid) {
    // Attempt SIGKILL on process group first, then individual PID
    if (kill(-pid, SIGKILL) != 0) {
        if (kill(pid, SIGKILL) != 0) {
            fprintf(stderr, "Error: Failed to force kill process %d\n", pid);
        }
    }
}

// Note: Increasing priority (negative nice values) requires sudo/root privileges
void renice_process(int pid, int new_priority) {
    if (setpriority(PRIO_PROCESS, pid, new_priority) != 0) {
        fprintf(stderr, "Error: Failed to renice process %d to %d\n", pid, new_priority);
    }
}