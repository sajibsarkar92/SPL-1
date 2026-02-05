// core.c — program controller

#include "core.h"
#include "cluster/clustering.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t file_access_mutex = PTHREAD_MUTEX_INITIALIZER;



// 1. The Unified Monitoring Thread Function


void *unified_monitor_thread_func(void *arg) {
    int interval = *((int *)arg);
    free(arg);

    ProcessInfo *old_list = NULL;
    SystemCpuInfo old_sys;
    int old_count = extract_processes(&old_list, &old_sys); // Initial Snap A

    while (1) {
        sleep(interval); // Wait for the window

        ProcessInfo *new_list = NULL;
        SystemCpuInfo new_sys;
        int new_count = extract_processes(&new_list, &new_sys); // Snap B

        pthread_mutex_lock(&file_access_mutex);
        
        // 1. Update Raw Data (using Snap B)
        print_raw_data_to_csv(new_list, new_count);
        write_system_info(new_sys);

        // 2. Update Resource Data (Compare A and B)
        int res_count = 0;
        ProcessResourceInfo *res_list = calculate_individual_resources(
            old_list, old_count, old_sys, new_list, new_count, new_sys, &res_count);
        if (res_list) { print_cal_processes_to_csv(res_list, res_count); free(res_list); }

        // 3. Update Aggregator Data (Compare A and B)
        AppSummary *summary_list = NULL;
        int app_count = aggregate_live_data(old_list, old_count, new_list, new_count, &old_sys, &new_sys, &summary_list);
        if (summary_list) { 
            print_aggregated_data_to_csv(summary_list, app_count); 
            perform_clustering_and_export(summary_list, app_count);
            free(summary_list); 
        }

        pthread_mutex_unlock(&file_access_mutex);

        // SLIDING WINDOW: B becomes A for next time
        free_process_list(old_list);
        old_list = new_list;
        old_sys = new_sys;
        old_count = new_count;
    }
    return NULL;
}

// launcher for the unified monitoring thread

pthread_t start_unified_monitoring(int interval) {
    pthread_t thread_id;
    int *arg = malloc(sizeof(int));
    if (!arg) return 0;
    *arg = interval;

    if (pthread_create(&thread_id, NULL, unified_monitor_thread_func, arg) != 0) {
        perror("Failed to create unified monitor thread");
        free(arg);
        return 0;
    }
    return thread_id;
}


