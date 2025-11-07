// main.c — minimal program entry point

#include "core.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {


    printf("Enter you choice :\n1. Run Process Monitor\n2. Export Raw Data to CSV\n");
    int choice;
    scanf("%d", &choice);
    switch(choice) {
        case 1:
            return run_porcess_monitor(1); // example interval of 5 seconds
        case 2:
            return raw_data_to_csv();
        default:
            fprintf(stderr, "Invalid choice. Exiting.\n");
            return EXIT_FAILURE;
    }


}


// the compile command to be used in terminal that includes all necessary files and flags: including header files and source files
// gcc -o process_monitor main.c core.c data_collector/data_collector.c data_colector/data_extractor.c -I. -std=c17
// Note: Ensure all paths are correct based on your project structure.
// also include .h files in the gcc command if necessary, the new commabd is:
// gcc -o process_monitor main.c core.c data_collector/data_collector.c data_collector/data_extractor.c -I. -std=c17