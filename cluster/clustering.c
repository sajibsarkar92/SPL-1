#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "clustering.h"

#define K_CLUSTERS 3
#define MAX_ITERATIONS 10

// Helper: Normalize values so CPU (0-100) and Mem (0-100) are treated equally
double calculate_distance(AppSummary *a, AppSummary *b) {
    double cpu_diff = a->cpu_percentage - b->cpu_percentage;
    double mem_diff = a->mem_percentage - b->mem_percentage;
    return sqrt(cpu_diff * cpu_diff + mem_diff * mem_diff);
}

void perform_clustering_and_export(AppSummary *apps, int count) {
    if (count < K_CLUSTERS) return;

    // 1. Initialize Medoids (Pick 3 random points)
    int medoid_indices[K_CLUSTERS] = {0, count / 2, count - 1}; 
    int *assignments = malloc(sizeof(int) * count);
    
    // 2. K-Medoids Algorithm Loop
    for (int iter = 0; iter < MAX_ITERATIONS; iter++) {
        int changed = 0;

        // Step A: Assign points to nearest Medoid
        for (int i = 0; i < count; i++) {
            double min_dist = DBL_MAX;
            int best_cluster = 0;

            for (int k = 0; k < K_CLUSTERS; k++) {
                double d = calculate_distance(&apps[i], &apps[medoid_indices[k]]);
                if (d < min_dist) {
                    min_dist = d;
                    best_cluster = k;
                }
            }
            if (assignments[i] != best_cluster) {
                assignments[i] = best_cluster;
                changed = 1;
            }
        }

        // Step B: Update Medoids (Find best representative)
        for (int k = 0; k < K_CLUSTERS; k++) {
            double best_sum_dist = DBL_MAX;
            int new_medoid_idx = medoid_indices[k];

            for (int candidate = 0; candidate < count; candidate++) {
                if (assignments[candidate] != k) continue;

                double current_sum_dist = 0;
                for (int other = 0; other < count; other++) {
                    if (assignments[other] == k) {
                        current_sum_dist += calculate_distance(&apps[candidate], &apps[other]);
                    }
                }

                if (current_sum_dist < best_sum_dist) {
                    best_sum_dist = current_sum_dist;
                    new_medoid_idx = candidate;
                }
            }
            medoid_indices[k] = new_medoid_idx;
        }

        if (!changed) break; 
    }

    // 3. Determine Cluster Labels based on the Medoids' stats
    char *labels[K_CLUSTERS];
    for(int k=0; k<K_CLUSTERS; k++) {
        double cpu = apps[medoid_indices[k]].cpu_percentage;
        double mem = apps[medoid_indices[k]].mem_percentage;
        
        if (cpu > 15.0 || mem > 15.0) labels[k] = "HIGH_IMPACT";
        else if (cpu > 2.0 || mem > 2.0) labels[k] = "MEDIUM_IMPACT";
        else labels[k] = "LOW_IMPACT";
    }

    // 4. PIPELINE OUTPUT: Write to NEW CSV
    FILE *fp = fopen("clustered_report.csv", "w");
    if (fp) {
        fprintf(fp, "Cluster_Label,App_Name,PID,CPU_Percent,Mem_Percent\n");
        
        // Write grouped data
        for (int k = 0; k < K_CLUSTERS; k++) {
            for (int i = 0; i < count; i++) {
                if (assignments[i] == k) {
                    fprintf(fp, "%s,%s,%d,%.2f,%.2f\n", 
                        labels[k],
                        apps[i].root_name,
                        apps[i].root_pid,
                        apps[i].cpu_percentage,
                        apps[i].mem_percentage
                    );
                }
            }
        }
        fclose(fp);
    }

    free(assignments);
}
