#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "data_collector.h"


int get_system_pid_max() {
    FILE *f = fopen("/proc/sys/kernel/pid_max", "r");
    int pid_max = 32768; // Default fallback
    if (f) {
        if (fscanf(f, "%d", &pid_max) != 1) pid_max = 32768;
        fclose(f);
    }
    // Safety clamp for unexpected values
    if (pid_max < 32768) pid_max = 32768; 
    return pid_max;

    if (pid_max > 4194304) pid_max = 4194304;
}


int load_raw_csv(char *filename, ProcessRaw **list_out){
    FILE *file = fopen(filename, "r");

    if(!file){
        perror("Error opening raw_data.csv for reading");
        return -1;
    }

    char line[1024];
    int count = 0;
    int capacity = 1024;

    ProcessRaw *list = malloc(capacity * sizeof(ProcessRaw));
    if(!list){
        perror("Memory allocation failed");
        fclose(file);
        return -1;      
    }

    // Skip header line
    fgets(line, sizeof(line), file);

    while(fgets(line,sizeof(line), file)){
        if(count >= capacity){
            capacity *=2;
            ProcessRaw *temp = realloc(list, capacity * sizeof(ProcessRaw));

            if(!temp){
                perror("Memory reallocation failed");
                free(list);
                fclose(file);
                return -1;
            }
            list = temp;     
        }
        ProcessRaw *p = &list[count];

        char temp_comm[128];
        long temp_utime, temp_stime, temp_rss; 
        
        
        int parsed = sscanf(line, 
            "%d,\"%127[^\"]\",\"%127[^\"]\",%d,%d,%d,%lu,%ld,%ld,%lu,%lu,%ld", 
            &p->pid, 
            p->name, 
            temp_comm, 
            &p->ppid, 
            &p->sid, 
            &p->uid, 
            &p->jiffies_total, 
            &temp_utime, &temp_stime, 
            &p->starttime,
            &p->pss_kb,
            &temp_rss 
        );

        
        if (parsed >= 10) { 
            count++;
        } else {
            fprintf(stderr, "Malformed line in CSV (Parsed %d/%d fields): %s", parsed, 12, line);
            
        }


    }
    fclose(file);
    *list_out = list;
    return count;

}

int read_sys_info(const char *filename, SystemSnap *info) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening system info file");
        return -1;
    }

    info->snapshot_wall_time = 0;
    info->uptime = 0.0L;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Snapshot Wall Time", 18) == 0) {
            sscanf(line, "%*[^:]: %ld", &info->snapshot_wall_time);
        }
    }
    
    fclose(f);
    if (info->snapshot_wall_time == 0) {
        info->snapshot_wall_time = time(NULL); 
    }
    
    return 0;
}




