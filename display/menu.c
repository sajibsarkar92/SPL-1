#include <stdio.h>
#include <stdlib.h>
#include "menu.h"
#include "controller/process_control.h"

void display_main_menu() {
    printf("\n=== Linux System Manager (Root Privileges Active) ===\n");
    printf("1. Suspend Process (Pause)\n");
    printf("2. Resume Process (Unpause)\n");
    printf("3. Terminate Process (Standard Kill)\n");
    printf("4. Force Kill (SIGKILL)\n");
    printf("5. Change Priority (High/Normal/Low)\n");
    printf("6. Exit\n");
    printf("----------------------------------------------------\n");
    printf("Enter choice: ");
}

void run_interactive_mode() {
    int choice;
    int target_pid;

    while (1) {
        display_main_menu();
        
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Clear buffer
            continue;
        }

        if (choice == 6) {
            printf("Exiting... Goodbye!\n");
            break; 
        }

        // Options 1-5 all require a PID
        if (choice >= 1 && choice <= 5) {
            printf("Enter Target Root PID from CSV: ");
            if (scanf("%d", &target_pid) != 1) {
                while (getchar() != '\n');
                printf("Invalid PID.\n");
                continue;
            }

            switch (choice) {
                case 1: 
                    suspend_process(target_pid); 
                    break;
                case 2: 
                    resume_process(target_pid); 
                    break;
                case 3: 
                    terminate_process(target_pid); 
                    break;
                case 4: 
                    kill_process(target_pid); 
                    break;
                case 5: {
                    int prio_choice;
                    printf("\n--- Select Priority Level for PID %d ---\n", target_pid);
                    printf("1. High Performance (Uses more CPU)\n");
                    printf("2. Normal (Standard Priority)\n");
                    printf("3. Low Power (Run in background)\n");
                    printf("Enter Level: ");
                    
                    if (scanf("%d", &prio_choice) == 1) {
                        int nice_val = 0;
                        if (prio_choice == 1) nice_val = -10;
                        else if (prio_choice == 3) nice_val = 15;
                        
                        // Map selection to your controller function
                        renice_process(target_pid, nice_val);
                    } else {
                        while (getchar() != '\n');
                        printf("Invalid selection.\n");
                    }
                    break;
                }
            }
        } else {
            printf("Invalid choice. Use 1-6.\n");
        }
    }
}