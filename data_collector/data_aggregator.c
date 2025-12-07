#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include "data_collector.h"

static int current_max_pid = 0;
static int *root_cache = NULL;
static ProcessInfo **pid_lookup = NULL;
static AppSummary *app_summaries = NULL;


void get_system_pid_max() {
    FILE *f = fopen("/proc/sys/kernel/pid_max", "r");
    int pid_max = 32768; // Default fallback
    if (f) {
        if (fscanf(f, "%d", &pid_max) != 1) pid_max = 32768;
        fclose(f);
    }
    // Safety clamp for unexpected values
    if (pid_max < 32768) pid_max = 32768; 
    current_max_pid = pid_max;

    if (pid_max > 4194304) pid_max = 4194304;
    current_max_pid = pid_max;
}


int load_raw_csv(char *filename, ProcessInfo **list_out){
    FILE *file = fopen(filename, "r");

    if(!file){
        perror("Error opening raw_data.csv for reading");
        return -1;
    }

    char line[1024];
    int count = 0;
    int capacity = 1024;

    ProcessInfo *list = malloc(capacity * sizeof(ProcessInfo));
    if(!list){
        perror("Memory allocation failed");
        fclose(file);
        return -1;      
    }

    // Skip header line
    fgets(line, sizeof(line), file);

    while(fgets(line,sizeof(line), file)){
        if(count >= capacity){
            capacity *=2;
            ProcessInfo *temp = realloc(list, capacity * sizeof(ProcessInfo));

            if(!temp){
                perror("Memory reallocation failed");
                free(list);
                fclose(file);
                return -1;
            }
            list = temp;     
        }
        ProcessInfo *p = &list[count];

        char temp_comm[128];
        long temp_utime, temp_stime, temp_rss,jiffies_ignored; 
        
        
        int parsed = sscanf(line, 
            "%d,\"%63[^\"]\",\"%63[^\"]\",%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld", 
            &p->pid, 
            p->name, 
            p->comm, 
            &p->ppid, 
            &p->sid, 
            &p->uid, 
            &jiffies_ignored, // The CSV has 'total_jiffies', but ProcessInfo has utime/stime
            &p->utime, 
            &p->stime, 
            &p->starttime,
            &p->pss_kb,
            &p->rss_kb 
        );
        p->delta_p = 0;
        
        if (parsed >= 12) { 
            count++;
        } else {
            fprintf(stderr, "Malformed line in CSV (Parsed %d/%d fields): %s", parsed, 12, line);
            
        }


    }
    fclose(file);
    *list_out = list;
    return count;

}

int read_sys_info(const char *filename, SystemSnap *info) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening system info file");
        return -1;
    }

    info->snapshot_wall_time = 0;
    info->uptime = 0.0L;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Snapshot Wall Time", 18) == 0) {
            sscanf(line, "%*[^:]: %ld", &info->snapshot_wall_time);
        }
    }
    
    fclose(f);
    if (info->snapshot_wall_time == 0) {
        info->snapshot_wall_time = time(NULL); 
    }
    
    return 0;
}




int is_shell(const char *name) {
    if (strcmp(name, "bash") == 0) return 1;
    if (strcmp(name, "sh") == 0) return 1;
    if (strcmp(name, "zsh") == 0) return 1;
    if (strcmp(name, "fish") == 0) return 1;
    if (strcmp(name, "gnome-shell") == 0) return 1; // GUI Shell
    return 0;
}


int get_app_root(int pid){

    if(pid <=0 || pid >= current_max_pid){
        return -1;
    }

    if(root_cache[pid] != -1){
        return root_cache[pid];
    }   

    // Traverse up the process tree
    // starting from pid until we find the root
    int index = pid;
    int root = pid;

    while(1){
        ProcessInfo *current_process = pid_lookup[index];

        if(current_process == NULL){
            root = index;
            break;
        } 

        
        // check for system or orphaned processes
        if(current_process ->pid == 1 || current_process ->ppid == 0 || current_process ->ppid == current_process ->pid){
            root = current_process ->pid;
            break;
        }

        ProcessInfo *parent_process = NULL;
        
        // check for valid parent pid
        if(current_process ->ppid > 0 && current_process ->ppid < current_max_pid){
            parent_process = pid_lookup[current_process ->ppid];
        }

        //if no parent found, we've reached the root
        if(parent_process == NULL){
            root = current_process ->pid;
            break;
        } else {
            // continue up the tree
            index = parent_process ->pid;
        }

        // check if user differ, if so, security wall reach, stop here
        if(current_process->uid != parent_process->uid){
            root = current_process ->pid;
            break;
        }


        // if parent is a shell and no curret, we have found the root app
        if(is_shell(parent_process->name) && !is_shell(current_process->name)){
            root = current_process ->pid;
            break;
        }

        // continue up the tree
        index = parent_process ->pid;
    }

    root_cache[pid] = root;
    return root;

}


int initialize_root_cache_and_lookup(){
    root_cache = malloc(sizeof(int) * current_max_pid);
    if(root_cache == NULL){
        perror("Failed to allocate memory for root cache");
        return -1;
    }

    for(int i=0; i<current_max_pid; i++){
        root_cache[i] = -1;
    }

    pid_lookup = malloc(sizeof(ProcessInfo*) * current_max_pid);
    if(pid_lookup == NULL){
        perror("Failed to allocate memory for PID lookup");
        free(root_cache);
        return -1;
    }

    for(int i=0; i<current_max_pid; i++){
        pid_lookup[i] = NULL;
    }

    return 0;
}


void build_pid_lookup(ProcessInfo *process_list, int process_count){
    for(int i=0; i<process_count; i++){
        ProcessInfo *p = &process_list[i];
        if(p->pid >0 && p->pid < current_max_pid){
            pid_lookup[p->pid] = p;
        }
    }
}


int build_AppSummary_list(ProcessInfo *process_list, int process_count, AppSummary **summary_list) {
    if (process_list == NULL || process_count <= 0) {
        return -1;
    }

    int capacity = 128;
    int count = 0;

    AppSummary *temp_list = malloc(sizeof(AppSummary) * capacity);
    if (temp_list == NULL) {
        perror("Failed to allocate memory for AppSummary list");
        return -1;
    }

    for (int i = 0; i < process_count; i++) {
        ProcessInfo *p = &process_list[i];

        int root_pid = get_app_root(p->pid);

        if (root_pid <= 0) {
            continue;
        }

        int index = -1;
        // Check if root_pid already exists in summary list
        for (int j = 0; j < count; j++) {
            if (temp_list[j].root_pid == root_pid) {
                index = j;
                break;
            }
        }

        // IF NEW APP FOUND
        if (index == -1) {
            // 1. Check if we need to resize
            if (count >= capacity) {
                capacity *= 2;
                AppSummary *new_list = realloc(temp_list, sizeof(AppSummary) * capacity);
                if (new_list == NULL) {
                    perror("Failed to reallocate memory for AppSummary list");
                    free(temp_list);
                    return -1;
                }
                temp_list = new_list;
            }

            // 2. Set index and increment count (THIS MUST HAPPEN ALWAYS FOR NEW APPS)
            index = count++;

            // 3. Initialize the new entry
            AppSummary *new_app = &temp_list[index];
            new_app->root_pid = root_pid;
            new_app->summed_pss_kb = 0;
            new_app->summed_delta_p = 0;
            new_app->total_processes = 0;

            if (pid_lookup[root_pid] != NULL) {
                strncpy(new_app->root_name, pid_lookup[root_pid]->name, sizeof(new_app->root_name) - 1);
            } else {
                snprintf(new_app->root_name, sizeof(new_app->root_name), "pid_%d", root_pid);
            }
            new_app->root_name[sizeof(new_app->root_name) - 1] = '\0';
        }

        // Accumulate data
        temp_list[index].summed_pss_kb += p->pss_kb;
        temp_list[index].summed_delta_p += p->delta_p;
        temp_list[index].total_processes += 1;
    }

    *summary_list = temp_list;
    return count;
}


