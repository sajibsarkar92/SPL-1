#include "core.h"
#include "display/menu.h" // Access to run_interactive_mode
#include <stdlib.h>
#include <stdio.h>

int main(void) {
    const int monitoring_interval = 4;

    // 1. Start the Engine (Background Thread)
    printf("🚀 Initializing System Monitor...\n");
    pthread_t monitor_thread = start_unified_monitoring(monitoring_interval);

    if (monitor_thread == 0) {
        fprintf(stderr, "Failed to start background monitor.\n");
        return EXIT_FAILURE;
    }
    printf("Background Service Active.\n");

    // 2. Start the Interface (Foreground Loop)
    // This function will block here until the user chooses "Exit"
    run_interactive_menu();

    // 3. Cleanup & Exit
    // (Optional: cancel thread if needed, or just let OS clean up)
    printf("Shutting down monitor thread...\n");
    return EXIT_SUCCESS;
}