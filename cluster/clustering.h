#ifndef CLUSTERING_H
#define CLUSTERING_H

#include "../data_collector/data_collector.h" // Import AppSummary definition

// Takes the aggregated list, groups them, and writes to "clustered_report.csv"
void perform_clustering_and_export(AppSummary *apps, int count);

#endif
