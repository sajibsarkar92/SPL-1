#ifndef CORE_H
#define CORE_H
#include "data_collector/data_collector.h"
/* Run the main controller. Returns 0 on success, non-zero on error. */

int run_porcess_monitor(int interval);
int raw_data_to_csv(void);

#endif 
