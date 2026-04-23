#ifndef CORE_H
#define CORE_H
#include "data_collector/data_collector.h"
#include <pthread.h>
extern pthread_mutex_t file_access_mutex;
extern int monitoring_interval;



int aggregate_raw_to_csv(void);
int raw_data_to_csv(void);
pthread_t start_unified_monitoring(int interval);
#endif
