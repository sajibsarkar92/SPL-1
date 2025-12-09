// core.c — program controller

#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t file_access_mutex = PTHREAD_MUTEX_INITIALIZER;



// 1. The Thread Function
void *raw_updater_thread_func(void *arg) {
    int interval = *((int *)arg);
    free(arg); // Clean up the malloc'd integer

    printf("🔄 Background Raw Data Updater started (Interval: %ds)...\n", interval);

    while (1) {
        // reuse your existing thread-safe function!
        // It handles extraction, printing, and cleanup internally.
        raw_data_to_csv(); 
        
        sleep(interval);
    }
    return NULL;
}

// 2. The Launcher Function
pthread_t start_raw_data_updater(int interval) {
    pthread_t thread_id;
    int *arg = malloc(sizeof(int));
    if (!arg) {
        perror("Failed to allocate memory for thread arg");
        return 0;
    }
    *arg = interval;

    if (pthread_create(&thread_id, NULL, raw_updater_thread_func, arg) != 0) {
        perror("Failed to create raw data updater thread");
        free(arg);
        return 0;
    }

    return thread_id;
}

void * monitor_thread  (void *arg) {
    int interval = *((int *)arg);
    while (1) {
        run_process_monitor(interval);
        // sleep(interval);
    }
    return NULL;
}


int raw_data_to_csv(void) {
    
    pthread_mutex_lock(&file_access_mutex);

    
    int result = export_raw_snapshot();

    pthread_mutex_unlock(&file_access_mutex);

    return result;
}

int aggregate_raw_to_csv(void) {
    
    pthread_mutex_lock(&file_access_mutex);

    
    int result = export_aggregated_snapshot();

    pthread_mutex_unlock(&file_access_mutex);

    return result;
}


int run_process_monitor(int interval) {
    ProcessInfo *proc_list1 = NULL;
    ProcessInfo *proc_list2 = NULL;
    SystemCpuInfo sys_info1;
    SystemCpuInfo sys_info2;
    int count1 = 0, count2 = 0;

    count1 = extract_processes(&proc_list1, &sys_info1);

    
    sleep(interval);


    count2 = extract_processes(&proc_list2, &sys_info2);

    if (count1 > 0 && count2 > 0) {
        int final_count = 0;
        ProcessResourceInfo *resource_list = calculate_individual_resources(
            proc_list1, count1, sys_info1,
            proc_list2, count2, sys_info2,
            &final_count
        );

        if (resource_list != NULL && final_count > 0) {
            printf("✅ Resource calculation completed for %d processes.\n", final_count);
            

			print_cal_processes_to_csv(resource_list, final_count);

            free(resource_list);
        } else {
            fprintf(stderr, "Warning: No valid process resource data could be calculated.\n");
        }

        // --- 7. CLEANUP RAW DATA (CRITICAL) ---
        // Release memory for the raw data lists collected by extract_processes
        free_process_list(proc_list1); // Assuming you have a function to free the list array
        free_process_list(proc_list2); // (If extract_processes returns a malloc'd array)
    } else {
        fprintf(stderr, "Error: Data collection failed on one or both snapshots.\n");
        // Ensure partial allocations are handled (e.g., if count1 > 0 but count2 == 0)
        if (proc_list1) free_process_list(proc_list1);
        if (proc_list2) free_process_list(proc_list2);
    }

    return 0; // Return success/failure code
}

pthread_t start_background_monitoring(int interval) {
    pthread_t thread_id;
    int *arg = malloc(sizeof(*arg));
    *arg = interval;
    if(pthread_create(&thread_id, NULL, monitor_thread, arg)!=0){
        perror("Failed to create monitoring thread");
        free(arg);
        return 0; // Indicate failure
    }
    return thread_id;
}