// main.c — minimal program entry point

#include "core.h"
#include <stdlib.h>
#include <stdio.h>

int main(void) {

    printf("Enter you choice :\n1. Run Process Monitor\n2. Export Raw Data to CSV\n3. Aggregate Raw CSV to aggregate.csv\n");
    int choice;
    scanf("%d", &choice);
    switch(choice) {
        case 1:
            return run_process_monitor(4); // example interval of 1 second
        case 2:
            return raw_data_to_csv();
        // case 3:
        //     return aggregate_raw_to_csv();
        default:
            fprintf(stderr, "Invalid choice. Exiting.\n");
            return EXIT_FAILURE;
    }


}


// compilatin command:
// gcc -I data_collector main.c core.c data_collector/data_extractor.c -o my_program