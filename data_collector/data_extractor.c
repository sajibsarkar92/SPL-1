#include "data_collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>


static int read_proc_stat(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    char temp_name[64];
    int read = fscanf(file, 
        "%*d "              //pid
        "%63s "             //name
        "%*c "              //statee
        "%d "               //ppid
        "%*d "              // extra
        "%d "               // sid
        "%*d %*d %*d "      // blah blah
        "%*lu %*lu %*lu %*lu " // blah balh
        "%ld "              //utime
        "%ld "              //stime
        "%*ld %*ld %*ld %*ld " //skip
        "%ld",              //startime
        temp_name,
        &info->ppid,
        &info->sid,         
        &info->utime,
        &info->stime,
        &info->starttime);

    // clean up name
    size_t len = strlen(temp_name);
    if (len > 0 && temp_name[0] == '(') {
        if (temp_name[len - 1] == ')') temp_name[len - 1] = '\0';  // Remove trailing ')'
        memmove(temp_name, temp_name + 1, strlen(temp_name));  // Remove leading '('
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


int read_total_system_memory(SystemCpuInfo *info) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) {
        perror("Error opening /proc/meminfo");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            unsigned long mem_kb;
            if (sscanf(line + 9, "%lu", &mem_kb) == 1) {
                info->total_mem_kb = mem_kb;
                fclose(file);
                return 0; // Success
            }
        }
    }

    fclose(file);
    return -1; 
}



// process status
int read_proc_status(pid_t pid, ProcessInfo *info){
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    
    FILE *file = fopen(path, "r");
    if (!file) {
        return -1;
    }
    
    char line[256];
    int rss_found = 0;
    int uid_found = 0;
    
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            long rss_kb;
            if (sscanf(line + 6, "%ld", &rss_kb) == 1) {
                info->rss_kb = rss_kb;
                rss_found = 1;
            }
        } 
        
        else if (strncmp(line, "Uid:", 4) == 0) {
            long uid;
            // Scan for the first (real) UID value
            if (sscanf(line + 4, " %ld", &uid) == 1) {
                info->uid = (uid_t)uid;
                uid_found = 1;
            }
        }
        

        if (rss_found && uid_found) {
            fclose(file);
            return 0;
        }
    }
    
    fclose(file);
    return (rss_found && uid_found) ? 0 : -1; 
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

// name, for better name

 int read_proc_cmdline(pid_t pid, ProcessInfo *info) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    
    FILE *file = fopen(path, "r");
    

    char cmdline_buffer[512] = {0}; 
    size_t len = 0;

    if (file) {
        len = fread(cmdline_buffer, 1, sizeof(cmdline_buffer) - 1, file);
        fclose(file);
    }
    
    if (len == 0) {
        
        strncpy(info->name, info->comm, sizeof(info->name) - 1);
        info->name[sizeof(info->name) - 1] = '\0';
        return 0;
    }
    
       
    
    char *command_path = cmdline_buffer; 

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

    if (read_system_uptime(system_info) != 0)
     return -1;

    if (read_total_system_memory(system_info) != 0)
     return -1;

    size_t capacity = 128; 
    ProcessInfo *temp_list = (ProcessInfo *)calloc(capacity, sizeof(ProcessInfo));
    
    if (temp_list == NULL) {
        perror("Failed to allocate initial memory for process list");
        return -1;
    }

    // open /proc
    if ((dir = opendir("/proc")) == NULL) {
        perror("Error opening /proc");
        free(temp_list); 
        return -1;
    }

    
    while ((entry = readdir(dir)) != NULL) {
        int is_pid = 1;
        for (char *c = entry->d_name; *c; c++) {
            if (!isdigit(*c)) {
                is_pid = 0;
                break;
            }
        }
        
        if (is_pid) {
            pid_t pid = (pid_t)atoi(entry->d_name);
            if (pid <= 0) continue;

            // vector
            if (count >= capacity) {
                size_t new_capacity = capacity * 2;
                ProcessInfo *tmp = realloc(temp_list, new_capacity * sizeof(ProcessInfo));
                
                if (tmp == NULL) {
                    break; 
                }
                
                temp_list = tmp;

               
                memset(temp_list + capacity, 0, (new_capacity - capacity) * sizeof(ProcessInfo));

                capacity = new_capacity;
            }

            ProcessInfo *current_info = &temp_list[count];
            memset(current_info, 0, sizeof(ProcessInfo)); 

            current_info->pid = pid;


            if (read_proc_stat(pid, current_info) == 0 &&
                read_proc_status(pid, current_info) == 0 &&
                read_proc_pss(pid, current_info) == 0 &&
                read_proc_cmdline(pid, current_info) == 0)  
            {
                count++;
            } else {
               
                memset(current_info, 0, sizeof(ProcessInfo));
            }
        }
    }

    closedir(dir);
    
    *list = temp_list;
    return (int)count;
}

#define CSV_FILENAME "raw_data.csv"




void print_raw_data_to_csv(ProcessInfo *list, int count) {
    FILE *file = fopen("raw_data.csv", "w");

    if (file == NULL) {
        perror("Error opening raw_data.csv for writing");
        return;
    }
    fprintf(file, "PID,NAME,COMM,PPID,SID,UID,Jiffies_Total,UTIME,STIME,STARTTIME,PSS_KB,VmRSS_KB\n");

   
    for (int i = 0; i < count; i++) {
        long jiffies_total = list[i].utime + list[i].stime;
        
        fprintf(file, "%d,\"%s\",\"%s\",%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld\n",
            list[i].pid,
            list[i].name,
            list[i].comm,
            list[i].ppid,
            list[i].sid,
            list[i].uid,        
            jiffies_total,     
            list[i].utime,
            list[i].stime,
            list[i].starttime,
            list[i].pss_kb,    
            list[i].rss_kb);
    }

    fclose(file);
}





int write_system_info(SystemCpuInfo sys_info){
    FILE *file = fopen("system_info.txt", "w");

    if(!file) return -1;

    unsigned long HZ = sysconf(_SC_CLK_TCK);

    fprintf(file, "Uptime (seconds): %Lf\n", sys_info.uptime);
    fprintf(file, "Ticks per second: %lu\n", HZ);
    fprintf(file, "Total Memory (KB): %lu\n", sys_info.total_mem_kb);

    fclose(file);

    return 0;
    
}

void free_process_list(ProcessInfo *list) {
    if (list != NULL) {
        free(list);
    }
}


int export_raw_snapshot(void) {
    ProcessInfo *proc_list = NULL;
    SystemCpuInfo sys_totals;
    int count;

    count = extract_processes(&proc_list, &sys_totals);

    if (count > 0) {
        
        print_raw_data_to_csv(proc_list, count);
        write_system_info(sys_totals);

        
        free_process_list(proc_list);
        return 0;
    } else if (count == 0) {
        
        free_process_list(proc_list);
        return 0; 
    } else {
        
        free_process_list(proc_list);
        return 1;
    }
}