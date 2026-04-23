#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h>
#include "process_control.h"

void suspend_process(int pid) {
    if (kill(pid, SIGSTOP) != 0) {
    
        fprintf(stderr, "Error: Failed to suspend process %d\n", pid);
    }
}

void resume_process(int pid) {
    if (kill(pid, SIGCONT) != 0) {
        fprintf(stderr, "Error: Failed to resume process %d\n", pid);
    }
}

void terminate_process(int pid) {
    if (kill(-pid, SIGTERM) != 0) {
        if (kill(pid, SIGTERM) != 0) {
            fprintf(stderr, "Error: Failed to terminate process %d\n", pid);
        }
    }
}

void force_kill(int pid) {
    if (kill(-pid, SIGKILL) != 0) {
        if (kill(pid, SIGKILL) != 0) {
            fprintf(stderr, "Error: Failed to force kill process %d\n", pid);
        }
    }
}

void renice_process(int pid, int new_priority) {
    if (setpriority(PRIO_PROCESS, pid, new_priority) != 0) {
        fprintf(stderr, "Error: Failed to renice process %d to %d\n", pid, new_priority);
    }
}