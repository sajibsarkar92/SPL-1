// core.c — program controller

#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include "data_collector/data_collector.h"

int raw_data_to_csv(void) {
	ProcessInfo *proc_list = NULL;
	SystemCpuInfo sys_totals;
	int count;

	printf("🚀 Starting raw data extraction...\n");

	count = extract_processes(&proc_list, &sys_totals);

	if (count > 0) {
		printf("✅ Successfully extracted %d processes.\n", count);
		print_raw_data_to_csv(proc_list, count, sys_totals);

		free(proc_list);
		return 0;
	} else if (count == 0) {
		printf("⚠️ No processes found.\n");
		free(proc_list);
		return 0; // not an error
	} else {
		fprintf(stderr, "❌ Extraction failed.\n");
		free(proc_list);
		return 1;
	}
}