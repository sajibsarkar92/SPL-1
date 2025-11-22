// main.c — minimal program entry point

#include "core.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // Include for sleep() if you want to keep the main thread busy

int main(void) {
    
    const int monitoring_interval = 4; 
    
    printf("Enter your choice :\n");
    printf("1. Run Process Monitor in BG\n");
    printf("2. Export Raw Data to CSV\n");
    printf("3. Aggregate Raw CSV to aggregate.csv (Not implemented)\n");
    
    int choice;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Invalid input format. Exiting.\n");
        return EXIT_FAILURE;
    }

    switch(choice) {
        case 1: {
            pthread_t monitor_thread_id = start_background_monitoring(monitoring_interval); 

            if (monitor_thread_id == 0) {
                return EXIT_FAILURE;
            }
            
            printf("--- Main thread running. Monitoring started in background. ---\n");
            
            // 3. CRITICAL: Prevent the main thread from exiting, which would kill the monitor thread.
            // This loop keeps the program alive and allows the main thread to do other work.
            while (1) {
                printf("Main thread active...\n");
                sleep(10);
                
            }
            
            break; 
        }
        case 2:
            // raw_data_to_csv() is assumed to return 0 on success.
            int result = raw_data_to_csv();
            // 2. Add break or use return
            return result;
        // case 3:
        //     return aggregate_raw_to_csv();
        default:
            fprintf(stderr, "Invalid choice. Exiting.\n");
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}