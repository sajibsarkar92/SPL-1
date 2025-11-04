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
    
    char temp_name[64];
    
    int read = fscanf(file, 
        "%*d "          // pid
        "%63s "         // comm
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
        temp_name,
        &info->utime,
        &info->stime,
        &info->cutime,
        &info->cstime,
        &info->starttime);

        // clean up the parentheses around the name
    size_t len = strlen(temp_name);
    if (len > 0 && temp_name[0] == '(') {
        temp_name[len - 1] = '\0';  // Remove trailing ')'
        memmove(temp_name, temp_name + 1, len - 1);  // Remove leading '('
    }
    strncpy(info->comm, temp_name, sizeof(info->comm) - 1);
    info->comm[sizeof(info->comm) - 1] = '\0';
    if (read != 6) {
    fclose(file);
    return -1;
    }
    fclose(file);
    return 0;
    
}

int read_cpu_info(SystemCpuInfo *cpu_info) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) {
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), file)) {
        // Parse the first line starting with "cpu"
        if (sscanf(line, "cpu %ld %ld %ld %ld",
                   &cpu_info->user,
                   &cpu_info->nice,
                   &cpu_info->system,
                   &cpu_info->idle) == 4) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return -1;
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
    return -1; 
}

// Helper function to read /proc/[pid]/cmdline for name

static int read_proc_cmdline(pid_t pid, ProcessInfo *info) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    FILE *file = fopen(path, "r");
    
    // Use a local buffer, large enough to read a long command path.
    char cmdline_buffer[512] = {0}; 
    size_t len = 0;

    if (file) {
        // Read the entire file content, which contains argv[0] followed by nulls.
        len = fread(cmdline_buffer, 1, sizeof(cmdline_buffer) - 1, file);
        fclose(file);
    }
    
   // If cmdline is empty, we fall back to using the 'comm' name.
    if (len == 0) {
        
        strncpy(info->name, info->comm, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        return 0;
    }
    
    // --- STEP B: Extract Basename from argv[0] ---
    // argv[0] is everything up to the first null character, which is exactly
    // what we read into cmdline_buffer because fread stops at the end of the file.
    char *command_path = cmdline_buffer; 

    // Find the basename: the part after the last '/'
    char *base = strrchr(command_path, '/');
    if (base != NULL) {
        // The base is the string immediately after the last '/'
        strncpy(info->name, base + 1, sizeof(info->name) - 1);
    } else {
        // No '/' found, use the entire command_path
        strncpy(info->name, command_path, sizeof(info->name) - 1);
    }
    
    // Ensure null termination and truncate to the size limit
    info->name[sizeof(info->name) - 1] = '\0'; 
    return 0;
}

/**
 * @brief Main function to extract all process data from /proc.
 * @param list Pointer to a ProcessInfo* list pointer (output).
 * @return The number of processes successfully extracted, or -1 on error.
 */

int extract_processes(ProcessInfo **list, SystemCpuInfo *system_info) {
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if (read_cpu_info(system_info) != 0) {
        // If system CPU totals cannot be read, the program cannot calculate CPU%.
        // It's best to exit cleanly here.
        return -1;
    }
    
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
                read_proc_cmdline(pid, current_info) == 0
            ) {
                
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

#define CSV_FILENAME "raw_data.csv"


/*

    temporary function to print raw data to csv file for testing

*/
void print_raw_data_to_csv(ProcessInfo *list, int count, SystemCpuInfo sys_info) {
    FILE *file = fopen(CSV_FILENAME, "w");
    if (file == NULL) {
        perror("Error opening raw_data.csv for writing");
        return;
    }

    // Print System CPU Totals (as a header/context row)
    fprintf(file, "SYSTEM_INFO,USER,NICE,SYSTEM,IDLE\n");
    fprintf(file, "CPU_JIFFIES,%ld,%ld,%ld,%ld\n\n",
        sys_info.user, sys_info.nice, sys_info.system, sys_info.idle);

    // Print Process Data Header
    fprintf(file, "PID,NAME,COMM,UTIME,STIME,CUTIME,CSTIME,STARTTIME,VmRSS_KB\n");

    // Print Process Rows
    for (int i = 0; i < count; i++) {
        fprintf(file, "%d,\"%s\",\"%s\",%ld,%ld,%ld,%ld,%ld,%ld\n",
            list[i].pid,
            list[i].name,
            list[i].comm,
            list[i].utime,
            list[i].stime,
            list[i].cutime,
            list[i].cstime,
            list[i].starttime,
            list[i].rss_kb);
    }

    fclose(file);
    printf("✅ Raw data written to %s successfully.\n", CSV_FILENAME);
}