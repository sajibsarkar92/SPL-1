// main.c — Program Entry Point

#include "core.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // For sleep()

int main(void) {
    
    // Define how often the monitors should update (in seconds)
    const int monitoring_interval = 4; 
    
    printf("\n=== Linux System Process Collector ===\n");
    printf("1. Start Full Background Monitoring Mode\n");
    printf("   (Launches: Raw Updater, Process Monitor, and Aggregator)\n");
    printf("2. Export Single Raw Data Snapshot (Manual)\n");
    printf("3. Run Aggregation on Existing Data (Manual)\n");
    printf("--------------------------------------\n");
    printf("Enter your choice: ");
    
    int choice;
    if (scanf("%d", &choice) != 1) {
        fprintf(stderr, "Invalid input format. Exiting.\n");
        return EXIT_FAILURE;
    }

    switch(choice) {
        case 1: {
            printf("\n🚀 Launching Background Services...\n");

            // 1. Start Raw Data Updater (Snapshots to raw_data.csv)
            pthread_t raw_thread = start_raw_data_updater(monitoring_interval);
            if (raw_thread == 0) {
                fprintf(stderr, "❌ Failed to start Raw Data Updater.\n");
                return EXIT_FAILURE;
            }
            printf("   ✅ Raw Data Updater started (PID Snapshots).\n");

            // 2. Start Individual Process Monitor (CPU/RAM to resource.csv)
            pthread_t monitor_thread = start_individual_process_monitoring(monitoring_interval);
            if (monitor_thread == 0) {
                fprintf(stderr, "❌ Failed to start Individual Process Monitor.\n");
                // Note: We don't exit here to allow partial functionality, or you can exit if strict.
                return EXIT_FAILURE;
            }
            printf("   ✅ Individual Process Monitor started (Resource Calculation).\n");

            // 3. Start Background Aggregator (App Groups to aggregated_data.csv)
            pthread_t agg_thread = start_bg_aggregation(monitoring_interval);
            if (agg_thread == 0) {
                 fprintf(stderr, "❌ Failed to start Aggregator.\n");
                 return EXIT_FAILURE;
            }
            printf("   ✅ Background Aggregator started (App Grouping).\n");
            
            printf("\n--- All systems running. Press Ctrl+C to stop. ---\n");
            
            // CRITICAL: Keep main thread alive so background threads don't die.
            while (1) {
                sleep(10); 
            }
            break; 
        }

        case 2: {
            printf("📸 Exporting Raw Data Snapshot...\n");
            // raw_data_to_csv() handles the mutex locking internally
            int result = raw_data_to_csv();
            
            if (result == 0) {
                printf("✅ Success! Data saved to 'raw_data.csv'.\n");
            } else {
                fprintf(stderr, "❌ Failed to export raw data.\n");
            }
            return result;
        }

        case 3: {
            printf("📊 Aggregating existing raw data...\n");
            // aggregate_raw_to_csv() handles the mutex locking internally
            int result = aggregate_raw_to_csv();
            
            if (result == 0) {
                printf("✅ Success! Report saved to 'aggregated_data.csv'.\n");
            } else {
                fprintf(stderr, "❌ Failed to generate aggregation report.\n");
            }
            return result;
        }

        default:
            fprintf(stderr, "Invalid choice. Exiting.\n");
            return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}