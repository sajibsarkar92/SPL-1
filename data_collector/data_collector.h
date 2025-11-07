// data_collector.h

#include <sys/types.h>

// this stores the basic struct for storing the process info one by one
typedef struct {
    pid_t pid;
    pid_t ppid;
    char name[64];   
    char comm[64];   // fallback name from /proc/[pid]/stat            
    long utime, stime, cutime, cstime; // for cpu calc
    long starttime;                    // Process start time (jiffies since boot)
    long rss_kb; 
    long pss_kb;                      // memory in kib
} ProcessInfo;

typedef struct {
    unsigned long  user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    long double uptime;
} SystemCpuInfo;


// Update extractor prototype to return both data types
int extract_processes(ProcessInfo **list, SystemCpuInfo *system_info);
void print_raw_data_to_csv(ProcessInfo *list, int count);
int write_system_info(SystemCpuInfo sys_info);