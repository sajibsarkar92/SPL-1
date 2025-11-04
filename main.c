//main entry point of the project


// temp for testing

// main.c

#include "data_collector.h"
#include <stdio.h>
#include <stdlib.h>

// Note: The implementations of extract_processes and print_raw_data_to_csv
// are in data_extractor.c.

int main() {
    ProcessInfo *proc_list = NULL;
    SystemCpuInfo sys_totals;
    int count;

    printf("🚀 Starting raw data extraction...\n");

    // Call the main extraction function
    count = extract_processes(&proc_list, &sys_totals);

    if (count > 0) {
        printf("✅ Successfully extracted %d processes.\n", count);
        
        // Print the raw data to CSV for verification
        print_raw_data_to_csv(proc_list, count, sys_totals);
        
        // Free the allocated memory
        free(proc_list);
    } else if (count == 0) {
        printf("⚠️ No processes found.\n");
    } else {
        fprintf(stderr, "❌ Extraction failed.\n");
    }

    return 0;
}