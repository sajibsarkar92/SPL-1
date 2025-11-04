//extracting raw data from /proc and save it in raw.csv file

// data_extractor.c

#include "data_collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// Helper function to read /proc/[pid]/stat
static int read_proc_stat(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    long dummy;
    
    int read = fscanf(file, 
        "%*d "          // pid
        "%*s "          // comm
        "%*c "          // state
        "%*d "          // ppid
        "%*d "          // pgrp
        "%*d "          // session
        "%*d "          // tty_nr
        "%*d "          // tpgid
        "%*u "          // flags
        "%*lu "         // minflt
        "%*lu "         // cminflt
        "%*lu "         // majflt
        "%*lu "         // cmajflt
        "%ld "          // utime
        "%ld "          // stime
        "%ld "          // cutime
        "%ld "          // cstime
        "%*ld "         // priority
        "%*ld "         // nice
        "%*ld "         // num_threads
        "%*ld "         // itrealvalue
        "%ld",          // starttime
        &info->utime,
        &info->stime,
        &info->cutime,
        &info->cstime,
        &info->starttime);

    
    if (read != 5) {
    fclose(file);
    return -1;
    }
    fclose(file);
    return 0;
    
}

// Helper function to read /proc/[pid]/status for VmRSS
static int read_proc_status(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            // Parse the VmRSS line
            long rss_kb;
            if (sscanf(line + 6, "%ld", &rss_kb) == 1) {
                info->rss_kb = rss_kb;
                fclose(file);
                return 0;
            }
        }
    }
    
    fclose(file);
    return -1; // VmRSS not found
}

// Helper function to read /proc/[pid]/cmdline for name
static int read_proc_cmdline(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    size_t len = fread(info->name, 1, sizeof(info->name) - 1, file);
    if (len > 0) {
        // cmdline is null-separated; replace nulls with spaces for readability
        for (size_t i = 0; i < len; i++) {
            if (info->name[i] == '\0') {
                info->name[i] = ' ';
            }
        }
        info->name[len] = '\0'; // Null-terminate the string
    } else {
        snprintf(info->name, sizeof(info->name), "[%d]", pid);
    }
    
    fclose(file);
    return 0;
}

/**
 * @brief Main function to extract all process data from /proc.
 * @param list Pointer to a ProcessInfo* list pointer (output).
 * @return The number of processes successfully extracted, or -1 on error.
 */

int extract_processes(ProcessInfo **list) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    
    // Use a dynamic array (realloc) to store the list
    ProcessInfo *temp_list = NULL;

    // 1. Open /proc
    if ((dir = opendir("/proc")) == NULL) {
        perror("Error opening /proc");
        return -1;
    }

    // 2. Iterate through all entries in /proc
    while ((entry = readdir(dir)) != NULL) {
        // Check if the directory name is purely numeric (a PID)
        int is_pid = 1;
        for (char *c = entry->d_name; *c; c++) {
            if (!isdigit(*c)) {
                is_pid = 0;
                break;
            }
        }
        
        if (is_pid) {
            pid_t pid = (pid_t)atoi(entry->d_name);
            
            // Resize the list to hold one more ProcessInfo
            temp_list = realloc(temp_list, (count + 1) * sizeof(ProcessInfo));
            if (temp_list == NULL) {
                // Handle realloc failure
                // (Need proper cleanup, but for simplicity, we exit loop here)
                break; 
            }
            
            // Initialize the new entry
            ProcessInfo *current_info = &temp_list[count];
            current_info->pid = pid;
            
            // 3. Read the necessary files (Core Logic)
            if (read_proc_stat(pid, current_info) == 0 &&
                read_proc_status(pid, current_info) == 0 &&
                read_proc_cmdline(pid, current_info) == 0) {
                
                count++;
            } else {
                // Clean up the failed allocation by reducing the list size
                // Note: Reallocating to (count) will effectively chop off the last element.
                temp_list = realloc(temp_list, count * sizeof(ProcessInfo));
            }
        }
    }

    closedir(dir);
    
    *list = temp_list;
    return count;
}