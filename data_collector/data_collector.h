// data_collector.h

#include <sys/types.h>

// this stores the basic struct for storing the process info one by one
typedef struct {
    pid_t pid;
    char name[64];   
    char comm[64];   // fallback name from /proc/[pid]/stat            
    long utime, stime, cutime, cstime; // for cpu calc
    long starttime;                    // Process start time (jiffies since boot)
    long rss_kb;                       // memory in kib
} ProcessInfo;

typedef struct {
    long user;
    long nice;
    long system;
    long idle;
    long double uptime;
} SystemCpuInfo;


// Update extractor prototype to return both data types
int extract_processes(ProcessInfo **list, SystemCpuInfo *system_info);
void print_raw_data_to_csv(ProcessInfo *list, int count, SystemCpuInfo sys_info);