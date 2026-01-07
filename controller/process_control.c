#include<signal.h>
#include <stdio.h>
#include <errno.h>
#include <sys/resource.h>


void suspend_process(int pid) {
    if(kill(pid, SIGSTOP) == 0){
        printf("Suspended process %d\n", pid);
    } else {
        perror("Failed to suspend process");
    }
}

void resume_process(int pid){
    if(kill(pid, SIGCONT) == 0){
        printf("Resumed process %d\n", pid);
    } else {
        perror("Failed to resume process");
    }
}

void terminate_process(int pid){
    if(kill(-pid, SIGTERM) == 0){
        printf("Killed to process group of %d\n", pid);
    } else if( kill(pid, SIGTERM) ==0){
        printf("Killed process %d\n", pid);
    } else {
        perror("Failed to terminate process");
    }
}

void force_kill(int pid){
    if(kill(-pid, SIGKILL) == 0){
        printf("Force killed process group of %d\n", pid);
    } else if( kill(pid, SIGKILL) ==0){
        printf("Force killed process %d\n", pid);
    } else {
        perror("Failed to force kill process");
    }
}
// sudo for high priority
void renice_process(int pid, int new_priority){
    if(setpriority(PRIO_PROCESS, pid, new_priority) == 0){
        printf("Reniced process %d to priority %d\n", pid, new_priority);
        
        if(new_priority < 0){
            printf("HIgher priority assigned. Requires appropriate permissions.\n");
        } else if(new_priority > 0){
            printf("Lower priority assigned.\n");
        } else {
            printf("Normal priority assigned.\n");
        }
    } else {
        perror("Failed to renice process");
    }
}