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
    int pid_max = 32768; 

    if (f) {
        if (fscanf(f, "%d", &pid_max) != 1) pid_max = 32768;
        fclose(f);
    }

    
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
            &jiffies_ignored,
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




static void calculate_and_update_deltas(ProcessInfo *list1, int count1, ProcessInfo *list2, int count2) {
    if (!list1 || !list2 || count1 <= 0 || count2 <= 0) return;

    memset(pid_lookup, 0, sizeof(ProcessInfo*) * current_max_pid);
    build_pid_lookup(list1, count1);

    for (int i = 0; i < count2; i++) {
        ProcessInfo *p2 = &list2[i];
        p2->delta_p = 0; 

        if (p2->pid <= 0 || p2->pid >= current_max_pid) continue;

        ProcessInfo *p1 = pid_lookup[p2->pid];

        if (p1 != NULL && p1->starttime == p2->starttime) {
             unsigned long total_ticks_1 = p1->utime + p1->stime;
             unsigned long total_ticks_2 = p2->utime + p2->stime;

             if (total_ticks_2 >= total_ticks_1) {
                 p2->delta_p = total_ticks_2 - total_ticks_1;
             }
        } else {
            // New process during the interval, set delta_p to total jiffies
            p2->delta_p = p2->utime + p2->stime;
        }
    }

    memset(pid_lookup, 0, sizeof(ProcessInfo*) * current_max_pid);
}


int aggregate_live_data(ProcessInfo *prev_list, int prev_count, 
                        ProcessInfo *curr_list, int curr_count, 
                        unsigned long system_total_ticks,  // <--- NEW ARG
                        unsigned long system_total_mem_kb, // <--- NEW ARG
                        AppSummary **summary_out) {


    if (root_cache == NULL || pid_lookup == NULL) {
        get_system_pid_max();
        if (initialize_root_cache_and_lookup() != 0) return -1;
    }

    
    calculate_and_update_deltas(prev_list, prev_count, curr_list, curr_count);


    build_pid_lookup(curr_list, curr_count);


    if (root_cache){
        memset(root_cache, -1, sizeof(int) * current_max_pid);
    } 


    int app_count = build_AppSummary_list(curr_list, curr_count, summary_out);

    
    if (app_count > 0 && *summary_out != NULL) {
        AppSummary *list = *summary_out;
        
        for (int i = 0; i < app_count; i++) {
            if (system_total_ticks > 0) {
                list[i].cpu_percentage = ((double)list[i].summed_delta_p / (double)system_total_ticks) * 100.0;
            } else {
                list[i].cpu_percentage = 0.0;
            }

            if (system_total_mem_kb > 0) {
                list[i].mem_percentage = ((double)list[i].summed_pss_kb / (double)system_total_mem_kb) * 100.0;
            } else {
                list[i].mem_percentage = 0.0;
            }
        }
    }

    return app_count;
}

void print_aggregated_data_to_csv(AppSummary *list, int count) {
    const char *filename = "aggregated_data.csv";
    FILE *file = fopen(filename, "w");
    
    if (file == NULL) {
        perror("Error opening aggregated_data.csv");
        return;
    }

    fprintf(file, "Root_PID,App_Name,Process_Count,Total_PSS_KB,Total_CPU_Ticks,CPU_Percent,Mem_Percent\n");

    for (int i = 0; i < count; i++) {
        fprintf(file, "%d,\"%s\",%d,%llu,%llu,%.2f,%.2f\n",
            list[i].root_pid,
            list[i].root_name,
            list[i].total_processes,
            list[i].summed_pss_kb,
            list[i].summed_delta_p,
            list[i].cpu_percentage,    // <--- Printed!
            list[i].mem_percentage     // <--- Printed!
        );
    }

    fclose(file);
    printf("✅ Aggregated data (with percentages) written to %s.\n", filename);
}