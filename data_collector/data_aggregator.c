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

// --- Helper Functions ---

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

int is_shell(const char *name) {
    if (strcmp(name, "bash") == 0) return 1;
    if (strcmp(name, "sh") == 0) return 1;
    if (strcmp(name, "zsh") == 0) return 1;
    if (strcmp(name, "fish") == 0) return 1;
    if (strcmp(name, "gnome-shell") == 0) return 1; 
    return 0;
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
        if(p->pid > 0 && p->pid < current_max_pid){
            pid_lookup[p->pid] = p;
        }
    }
}

int get_app_root(int pid){
    if(pid <= 0 || pid >= current_max_pid){
        return -1;
    }

    if(root_cache[pid] != -1){
        return root_cache[pid];
    }   

    int index = pid;
    int root = pid;

    while(1){
        ProcessInfo *current_process = pid_lookup[index];

        if(current_process == NULL){
            root = index;
            break;
        } 

        // check for system or orphaned processes
        if(current_process->pid == 1 || current_process->ppid == 0 || current_process->ppid == current_process->pid){
            root = current_process->pid;
            break;
        }

        ProcessInfo *parent_process = NULL;
        
        if(current_process->ppid > 0 && current_process->ppid < current_max_pid){
            parent_process = pid_lookup[current_process->ppid];
        }

        if(parent_process == NULL){
            root = current_process->pid;
            break;
        } else {
            index = parent_process->pid;
        }

        // Security boundary check
        if(current_process->uid != parent_process->uid){
            root = current_process->pid;
            break;
        }

        // Shell check
        if(is_shell(parent_process->name) && !is_shell(current_process->name)){
            root = current_process->pid;
            break;
        }

        index = parent_process->pid;
    }

    root_cache[pid] = root;
    return root;
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
        // Simple linear search for existing app entry
        for (int j = 0; j < count; j++) {
            if (temp_list[j].root_pid == root_pid) {
                index = j;
                break;
            }
        }

        // IF NEW APP FOUND
        if (index == -1) {
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

            index = count++;

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

        temp_list[index].summed_pss_kb += p->pss_kb;
        temp_list[index].summed_delta_p += p->delta_p;
        temp_list[index].total_processes += 1;
    }

    *summary_list = temp_list;
    return count;
}

static void calculate_and_update_deltas(ProcessInfo *list1, int count1, ProcessInfo *list2, int count2) {
    if (!list1 || !list2 || count1 <= 0 || count2 <= 0) return;

    // Reset lookup for the PREVIOUS list
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
            // New process during the interval
            p2->delta_p = p2->utime + p2->stime;
        }
    }

    // Clean up lookup
    memset(pid_lookup, 0, sizeof(ProcessInfo*) * current_max_pid);
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
            list[i].cpu_percentage,
            list[i].mem_percentage
        );
    }

    fclose(file);
    printf("✅ Aggregated data written to %s.\n", filename);
}

// --- MAIN AGGREGATION LOGIC ---

/* * Calculates Deltas and Percentages based on two SystemCpuInfo snapshots.
 * Returns the count of aggregated applications.
 */
int aggregate_live_data(ProcessInfo *prev_list, int prev_count, 
                        ProcessInfo *curr_list, int curr_count, 
                        SystemCpuInfo *prev_sys, 
                        SystemCpuInfo *curr_sys, 
                        AppSummary **summary_out) {

    // 1. Calculate System Time Diff (The Denominator for CPU %)
    unsigned long system_total_ticks = 0;
    long double uptime_diff = curr_sys->uptime - prev_sys->uptime;
    
    // Safety check
    if (uptime_diff <= 0.0001L) {
        uptime_diff = 1.0L; 
    }

    // Convert seconds to ticks (jiffies)
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    system_total_ticks = (unsigned long)(uptime_diff * ticks_per_sec);

    // 2. Initialize Helpers
    if (root_cache == NULL || pid_lookup == NULL) {
        get_system_pid_max();
        if (initialize_root_cache_and_lookup() != 0) return -1;
    }

    // 3. Calculate Process Deltas (Numerator)
    calculate_and_update_deltas(prev_list, prev_count, curr_list, curr_count);

    // 4. Build App List (Group by Root PID)
    // Clear lookup for current list build
    build_pid_lookup(curr_list, curr_count);
    
    if (root_cache){
        memset(root_cache, -1, sizeof(int) * current_max_pid);
    } 

    int app_count = build_AppSummary_list(curr_list, curr_count, summary_out);

    // 5. Calculate Percentages
    if (app_count > 0 && *summary_out != NULL) {
        AppSummary *list = *summary_out;
        
        for (int i = 0; i < app_count; i++) {
            // CPU Percentage = (App Delta / System Delta) * 100
            if (system_total_ticks > 0) {
                list[i].cpu_percentage = ((double)list[i].summed_delta_p / (double)system_total_ticks) * 100.0;
            } else {
                list[i].cpu_percentage = 0.0;
            }

            // Memory Percentage = (App PSS / System Total Mem) * 100
            if (curr_sys->total_mem_kb > 0) {
                list[i].mem_percentage = ((double)list[i].summed_pss_kb / (double)curr_sys->total_mem_kb) * 100.0;
            } else {
                list[i].mem_percentage = 0.0;
            }
        }
    }

    return app_count;
}

// --- PUBLIC API FOR UNIFIED THREAD ---

/* * Called by core.c (Unified Thread). 
 * Accepts raw snapshots, performs aggregation, and prints to CSV.
 */
void update_aggregated_data_csv(ProcessInfo *prev_list, int prev_count, 
                                ProcessInfo *curr_list, int curr_count,
                                SystemCpuInfo *prev_sys, SystemCpuInfo *curr_sys) {
    
    AppSummary *summary_list = NULL;
    
    int app_count = aggregate_live_data(
        prev_list, prev_count, 
        curr_list, curr_count, 
        prev_sys, 
        curr_sys, 
        &summary_list
    );

    if (app_count > 0 && summary_list != NULL) {
        print_aggregated_data_to_csv(summary_list, app_count);
        free(summary_list);
    } 
}

// --- MANUAL OPTION 3 (Active Sampling) ---

/*
 * Called by main.c -> core.c (Option 3).
 * Performs "Active In-Memory Sampling" to ensure accurate CPU calculation
 * without relying on (or conflicting with) CSV files.
 */
int export_aggregated_snapshot(void) {
    ProcessInfo *list1 = NULL;
    ProcessInfo *list2 = NULL;
    SystemCpuInfo sys_info1;
    SystemCpuInfo sys_info2;
    int count1 = 0, count2 = 0;

    printf("📊 Aggregator: Capturing baseline snapshot (T0)...\n");
    count1 = extract_processes(&list1, &sys_info1);
    if (count1 < 0) {
        fprintf(stderr, "❌ Failed to capture baseline snapshot.\n");
        return 1;
    }

    printf("⏳ Sampling system activity for 2 seconds (to calc CPU %)...\n");
    sleep(2); 

    printf("📊 Aggregator: Capturing second snapshot (T1)...\n");
    count2 = extract_processes(&list2, &sys_info2);
    if (count2 < 0) {
        fprintf(stderr, "❌ Failed to capture second snapshot.\n");
        free_process_list(list1);
        return 1;
    }

    // Reuse the public API logic
    update_aggregated_data_csv(list1, count1, list2, count2, &sys_info1, &sys_info2);

    // Cleanup
    free_process_list(list1);
    free_process_list(list2);

    return 0;
}