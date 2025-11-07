#ifndef DATA_COLLECTOR_H
#define DATA_COLLECTOR_H

#include <sys/types.h>
#include <stddef.h>

/* Process & system structures */
typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[64];
    char comm[64];
    long utime, stime, cutime, cstime;
    long starttime;
    long rss_kb;
    long pss_kb;
} ProcessInfo;

typedef struct {
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long total_mem_kb;
    long double uptime;
} SystemCpuInfo;

typedef struct {
    int pid;
    int ppid;
    char name[64];
    long starttime;
    long double cpu_percentage;
    long double current_process_uptime;
    unsigned long rss_kb;
    unsigned long pss_kb;
    long double rss_percentage;
    long double pss_percentage;
    long double aggregated_cpu_percentage;
    long double aggregated_pss_percentage;
} ProcessResourceInfo;

typedef struct HashMapNode {
    pid_t pid;
    long starttime;
    unsigned long key;
    ProcessInfo *value; /* stores a snapshot (ProcessInfo) from list1 */
    struct HashMapNode *next;
} HashMapNode;

/* Public API (prototypes) */
int extract_processes(ProcessInfo **list, SystemCpuInfo *system_info);
void print_raw_data_to_csv(ProcessInfo *list, int count);
int write_system_info(SystemCpuInfo sys_info);

/* Resource calculator API */
void HashMapBuilder(ProcessInfo *list, int count); /* single pointer */
unsigned long hashKey(int pid, long starttime);
ProcessResourceInfo* calculate_individual_resources(
    ProcessInfo *list1, int count1, SystemCpuInfo sys_info1,
    ProcessInfo *list2, int count2, SystemCpuInfo sys_info2,
    int *final_count_ptr
);
void print_cal_processes_to_csv(const ProcessResourceInfo *list, int count);
void freeHashMap(void);

/* Utilities */
void free_process_list(ProcessInfo *list);

#endif /* DATA_COLLECTOR_H */