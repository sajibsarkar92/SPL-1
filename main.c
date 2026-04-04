#include "core.h"
#include "display/menu.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h> // for sleep

int main(void) {
    const int monitoring_interval = 4;

    // 1. Start Background Monitor
    // Note: We use printf here BEFORE ncurses starts, which is fine.
    printf("🚀 Initializing Pulse Engine...\n");
    pthread_t monitor_thread = start_unified_monitoring(monitoring_interval);

    if (monitor_thread == 0) {
        fprintf(stderr, "Failed to start background monitor.\n");
        return EXIT_FAILURE;
    }
    
    // Give the engine a moment to generate the first CSV
    printf("Waiting for data stream...\n");
    sleep(1);

    // 2. Start Ncurses UI (Takes over the terminal)
    run_interactive_menu();

    // 3. Cleanup
    // When run_interactive_menu returns (user pressed 'q'), we exit.
    // The OS will clean up the background thread.
    return EXIT_SUCCESS;
}