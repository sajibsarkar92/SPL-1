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

void *individual_process_monitor_thread_func(void *arg) {
    int interval = *((int *)arg);
    free(arg); 
    
    printf("👀 Individual Process Monitor Thread started (Interval: %ds)...\n", interval);
    
    while (1) {
        // Calls the logic in resource_calculator.c
        monitor_individual_processes_cycle(interval);
    }
    return NULL;
}

void *aggregator_thread_func(void *arg){
    int interval = *(int*) arg;
    free(arg);

    while(1){
        aggregate_raw_to_csv();

        sleep(interval);
    }
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

pthread_t start_individual_process_monitoring(int interval) {
    pthread_t thread_id;
    int *arg = malloc(sizeof(int));
    if (!arg) {
        perror("Failed to allocate memory for thread arg");
        return 0;
    }
    *arg = interval;

    if (pthread_create(&thread_id, NULL, individual_process_monitor_thread_func, arg) != 0) {
        perror("Failed to create individual process monitor thread");
        free(arg);
        return 0;
    }

    return thread_id;
}

pthread_t start_bg_aggregation(int interval){
    pthread_t thread_id;
    int *arg = malloc (sizeof(int));

    if(!arg){
        perror("Failed to allocate memory for thread arg");
        return 0;
    }
    *arg = interval;

    if(pthread_create(&thread_id, NULL, aggregator_thread_func, arg) !=0){
        perror("Failed to create aggregator thread");
        free(arg);
        return 0;
    }

    return thread_id;
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



