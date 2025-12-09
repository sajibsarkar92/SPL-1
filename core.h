#ifndef CORE_H
#define CORE_H
#include "data_collector.h"
#include <pthread.h>
/* Run the main controller. Returns 0 on success, non-zero on error. */
extern pthread_mutex_t file_access_mutex;
extern int monitoring_interval;
int run_process_monitor(int interval);
int raw_data_to_csv(void);

/* new: aggregate raw CSV into aggregate.csv */
int aggregate_raw_to_csv(void);
pthread_t start_background_monitoring(int interval);
pthread_t start_raw_data_updater(int interval);
#endif
