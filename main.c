#include "core.h"
#include "display/menu.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> 

int main(void) {
    const int monitoring_interval = 4;


    printf("Starting Pulse Engine...\n");
    pthread_t monitor_thread = start_unified_monitoring(monitoring_interval);

    if (monitor_thread == 0) {
        fprintf(stderr, "Failed to start background monitor.\n");
        return EXIT_FAILURE;
    }

    printf("Waiting for data stream...\n");
    sleep(1);

    run_interactive_menu();

   
    return EXIT_SUCCESS;
}