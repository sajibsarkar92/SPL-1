#include "core.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// Optional: A clean way to handle Ctrl+C
void handle_sigint(int sig) {
    printf("\n👋 Shutting down System Monitor safely...\n");
    exit(0);
}

int main(void) {
    // The "Sliding Window" interval (e.g., 4 seconds)
    const int monitoring_interval = 4; 

    // Handle Ctrl+C gracefully
    signal(SIGINT, handle_sigint);

    printf("==========================================\n");
    printf("🚀 Linux System Monitor: Unified Mode\n");
    printf("==========================================\n");
    printf("📊 Status: Initializing background thread...\n");

    // --- STEP 1: Launch the Master Monitor ---
    // This starts the "Sliding Window" logic in core.c
    pthread_t monitor_thread = start_unified_monitoring(monitoring_interval);

    if (monitor_thread == 0) {
        fprintf(stderr, "❌ Error: Failed to launch monitoring thread.\n");
        return EXIT_FAILURE;
    }

    printf("✅ Monitoring Active!\n");
    printf("📂 Updating: raw_data.csv, resource.csv, aggregated_data.csv\n");
    printf("⏱️ Interval: Every %d seconds\n", monitoring_interval);
    printf("💡 Press [Ctrl+C] to stop monitoring.\n");
    printf("==========================================\n");

    // --- STEP 2: Keep-Alive Loop ---
    // Since we don't have a menu, the main thread must stay alive.
    // We use a long sleep in a loop to consume near-zero CPU.
    while (1) {
        sleep(3600); // Wake up once an hour just to check if we're still running
    }

    return EXIT_SUCCESS;
}