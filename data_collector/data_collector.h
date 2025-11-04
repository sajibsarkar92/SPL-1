// data_collector.h

#include <sys/types.h>

// this stores the basic struct for storing the process info one by one
typedef struct {
    pid_t pid;
    char name[64];                      
    long utime, stime, cutime, cstime; // for cpu calc
    long starttime;                    // Process start time (jiffies since boot)
    long rss_kb;                       // memory in kib
} ProcessInfo;


// The caller is responsible for freeing the memory allocated for the list.
int extract_processes(ProcessInfo **list);