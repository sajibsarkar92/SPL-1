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
        "%d "          // ppid
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
        &info->ppid,
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
    if (read != 7) {
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
        if (sscanf(line, "cpu %lu %lu %lu %lu",
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

int read_system_uptime(SystemCpuInfo *cpu_info){
    FILE *file = fopen("/proc/uptime", "r");
    if (!file) {
        return -1;
    }

    if (fscanf(file, "%Lf", &cpu_info->uptime) != 1) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

// data_collector/data_extractor.c

int read_total_system_memory(SystemCpuInfo *info) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        perror("Error opening /proc/meminfo");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Look for the "MemTotal" line
        if (strncmp(line, "MemTotal:", 9) == 0) {
            unsigned long mem_kb;
            // Scan for the value (it should be in KB)
            if (sscanf(line + 9, "%lu", &mem_kb) == 1) {
                info->total_mem_kb = mem_kb;
                fclose(file);
                return 0; // Success
            }
        }
    }

    fclose(file);
    return -1; // Failure to find MemTotal
}





// 
int read_proc_status(pid_t pid, ProcessInfo *info){
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



int read_proc_pss(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/smaps", pid);     
    FILE *fp = fopen(path, "r");
    
    if(!fp) {
        info->pss_kb = 0; 
        return -1;
    }

    char line[256];
    info->pss_kb = 0; 
    while(fgets(line, sizeof(line), fp)){
            if(strncmp(line, "Pss:", 4) == 0){ 
            long pss;
                        if(sscanf(line + 4, "%ld", &pss) == 1){ 
                info->pss_kb += pss;
            }
        }
    }
    
    fclose(fp);
    return 0; 
}

// Helper function to read /proc/[pid]/cmdline for name

 int read_proc_cmdline(pid_t pid, ProcessInfo *info) {
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



int extract_processes(ProcessInfo **list, SystemCpuInfo *system_info) {
    DIR *dir;
    struct dirent *entry;
    size_t count = 0;

    if (read_cpu_info(system_info) != 0) {
        // upon failure, exit here
        return -1;
    }
    if(read_system_uptime(system_info) != 0){
        // same here
        return -1;
    }

    

    if(read_total_system_memory(system_info) != 0) {
        return -1;
    }
    
    // realloc to allocate, and use temporary pointer to avoid leaks on failure
    ProcessInfo *temp_list = NULL;
    size_t capacity = 0; // current allocated capacity

    // 1. Opening /proc
    if ((dir = opendir("/proc")) == NULL) {
        perror("Error opening /proc");
        return -1;
    }

    // 2. Iterate through all entries in /proc
    while ((entry = readdir(dir)) != NULL) {
        // check if the folder is numeric
        int is_pid = 1;
        for (char *c = entry->d_name; *c; c++) {
            if (!isdigit(*c)) {
                is_pid = 0;
                break;
            }
        }
        
        if (is_pid) {
            pid_t pid = (pid_t)atoi(entry->d_name);

            // making sure we have enough space
            if ((count + 1) > capacity) {
                size_t new_capacity;
                if (capacity == 0) {
                    new_capacity = 64; // initial capacity
                } else {
                    new_capacity = capacity * 2; // double the capacity, like vector
                }


                if ((count + 1) > new_capacity) {
                    new_capacity = (size_t)(count + 1);
                }

                /* Use temporary pointer to avoid losing temp_list on failure. */
                ProcessInfo *tmp = realloc(temp_list, new_capacity * sizeof(ProcessInfo));
                if (tmp == NULL) {
                    // upon failure, skip this PID but keep existing data
                    continue;
                }
                // Success, update pointer and capacity
                temp_list = tmp;
                //update capacity
                capacity = new_capacity;
            }

            // Fill in the ProcessInfo structure
            ProcessInfo *current_info = &temp_list[count];
            current_info->pid = pid;

            // helper functins to read data from various /proc files
            if (read_proc_stat(pid, current_info) == 0 &&
                read_proc_status(pid, current_info) == 0 &&
                read_proc_pss(pid, current_info) == 0 &&
                read_proc_cmdline(pid, current_info) == 0)  
            {
                count++;
            } else {
                // on failure, skip this process but keep existing data
                continue;
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


void print_raw_data_to_csv(ProcessInfo *list, int count) {
    FILE *file = fopen(CSV_FILENAME, "w");
    if (file == NULL) {
        perror("Error opening raw_data.csv for writing");
        return;
    }

    // Print System CPU Totals (as a header/context row)
    // fprintf(file, "SYSTEM_INFO,USER,NICE,SYSTEM,IDLE,UPTIME,\n");
    // fprintf(file, "CPU_JIFFIES,%ld,%ld,%ld,%ld,%Lf\n\n",
    //     sys_info.user, sys_info.nice, sys_info.system, sys_info.idle,sys_info.uptime);

    // Print Process Data Header
    fprintf(file, "PID,NAME,COMM,PPID,UTIME,STIME,CUTIME,CSTIME,STARTTIME,VmRSS_KB\n");

    // Print Process Rows
    for (int i = 0; i < count; i++) {
        fprintf(file, "%d,\"%s\",\"%s\",%d,%ld,%ld,%ld,%ld,%ld,%ld\n",
            list[i].pid,
            list[i].name,
            list[i].comm,
            list[i].ppid,
            list[i].utime,
            list[i].stime,
            list[i].cutime,
            list[i].cstime,
            list[i].starttime,
            list[i].rss_kb);
    }

    fclose(file);
    printf(" Raw data written to %s successfully.\n", CSV_FILENAME);
}




// Updated function to include SystemCpuInfo
int write_system_info(SystemCpuInfo sys_info){
    FILE *file = fopen("system_info.txt", "w");

    if(!file) return -1;

    unsigned long HZ = sysconf(_SC_CLK_TCK);

    fprintf(file, "System CPU Information:\n");
    fprintf(file, "User Jiffies: %lu\n", sys_info.user);
    fprintf(file, "Nice Jiffies: %lu\n", sys_info.nice);
    fprintf(file, "System Jiffies: %lu\n", sys_info.system);
    fprintf(file, "Idle Jiffies: %lu\n", sys_info.idle);
    fprintf(file, "Uptime (seconds): %Lf\n", sys_info.uptime);
    fprintf(file, "Ticks per second: %lu\n", HZ);
    fprintf(file, "Total Memory (KB): %lu\n", sys_info.total_mem_kb);

    fclose(file);

    return 0;
    
}

/* Free a process list previously returned by extract_processes */
void free_process_list(ProcessInfo *list) {
    if (list != NULL) {
        free(list);
    }
}