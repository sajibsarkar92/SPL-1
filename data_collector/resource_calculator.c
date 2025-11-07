// calculate the ram, uptime, cpu usage from the data. Application wise  and save it in resource.csv file
#include "data_collector.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h> // added for memcpy/strncpy

#define HASH_MAP_BUCKETS 100003 // A large prime number for better distribution

static HashMapNode* hashTable[HASH_MAP_BUCKETS]; // single global table

// from murmurhash-like operations
unsigned long hashKey(int pid, long starttime) {
    unsigned long hash = (unsigned long)starttime * 0xc6a4a7935bd1e995UL;
    hash ^= hash >> 47;
    hash *= 0x9e3779b97f4a7c15UL;
    hash ^= (unsigned long)pid;
    hash *= 0xbf58476d1ce4e5b9UL;
    hash ^= hash >> 47;
    return hash;
}

HashMapNode* createHashNode(ProcessInfo *process){

    HashMapNode *newNode = (HashMapNode *) malloc (sizeof(HashMapNode));
    if(!newNode){
        return NULL;
    }

    newNode->pid = process->pid;
    newNode->starttime = process->starttime;
    newNode->key = hashKey(process->pid, process->starttime);
    newNode->next = NULL;

    /* Allocate and copy a ProcessInfo snapshot (value) */
    newNode->value = (ProcessInfo*) malloc (sizeof(ProcessInfo));
    if(newNode->value == NULL){
        free(newNode);
        return NULL;
    }
    /* shallow copy is fine for this struct (contains arrays and scalars) */
    *(newNode->value) = *process;

    return newNode;
}


void HashMapBuilder(ProcessInfo *list, int count){
    if (list == NULL || count <= 0){
        return;
    }

    for (int i = 0; i < HASH_MAP_BUCKETS; i++) {
        hashTable[i] = NULL;
    }

    for(int i = 0; i <count; i++){

        HashMapNode *newNode = createHashNode(&list[i]);

        if (newNode == NULL){
            continue;
        }
        unsigned long index = newNode->key % HASH_MAP_BUCKETS;

        if(hashTable[index] == NULL){
            hashTable[index] = newNode;
        } else {
            // collision handling like linked list
            HashMapNode *current = hashTable[index];
            while(current->next != NULL){
                current = current->next;
            }
            current->next = newNode;
        }
    }
}

ProcessInfo* compareProcessInfo(ProcessInfo *a){
    unsigned long key = hashKey(a->pid, a->starttime);

    unsigned long index = key % HASH_MAP_BUCKETS;

    HashMapNode *current = hashTable[index];

    while(current != NULL){
        if(current->pid == a->pid && current->starttime == a->starttime){
            return current->value; // found (ProcessInfo*)
        }
        current = current->next;
    }
    return NULL; 
}


// Free entire hash table (nodes and their ProcessInfo values)
void freeHashMap(void) {
    for (int i = 0; i < HASH_MAP_BUCKETS; i++) {
        HashMapNode *current = hashTable[i];
        
        while (current != NULL) {
            HashMapNode *next_node = current->next;
            
            if (current->value != NULL) {
                free(current->value); 
            }
            free(current);
            
            current = next_node;
        }
        hashTable[i] = NULL;
    }
    // optional: printf("Hash map memory successfully freed.\n");
}


ProcessResourceInfo* calculate_individual_resources(
    ProcessInfo *list1, int count1, SystemCpuInfo sys_info1,
    ProcessInfo *list2, int count2, SystemCpuInfo sys_info2,
    int *final_count_ptr 
) {
    if (list1 == NULL || list2 == NULL || count1 <= 0 || count2 <= 0) {
        if (final_count_ptr) *final_count_ptr = 0;
        return NULL;
    }

    unsigned long total_system_jiffies1 = sys_info1.user + sys_info1.nice + sys_info1.system + sys_info1.idle;
    unsigned long total_system_jiffies2 = sys_info2.user + sys_info2.nice + sys_info2.system + sys_info2.idle  ;
    unsigned long delta_system_jiffies = total_system_jiffies2 - total_system_jiffies1;
    
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (cores < 1) cores = 1;
    long ticks_per_second = sysconf(_SC_CLK_TCK); // HZ

    ProcessResourceInfo *final_list = NULL;
    int calculated_count = 0;
    int capacity = 64; 
    
    final_list = (ProcessResourceInfo *) malloc (capacity * sizeof(ProcessResourceInfo));
    if(final_list == NULL){
        perror("Memory allocation failed for final list");
        if (final_count_ptr) *final_count_ptr = 0;
        return NULL;
    }

    HashMapBuilder(list1, count1);

    for(int i = 0; i < count2; i++){
        ProcessInfo *proc2 = &list2[i];
        ProcessInfo *proc1 = compareProcessInfo(proc2);

        if(proc1 != NULL){
            
            if(calculated_count >= capacity){
                capacity *= 2;
                ProcessResourceInfo *temp = realloc(final_list, capacity * sizeof(ProcessResourceInfo));
                if(temp == NULL){
                    perror("Memory reallocation failed");
                    break;
                }
                final_list = temp;
            }

            ProcessResourceInfo *res = &final_list[calculated_count];

            unsigned long total_proc_jiffies1 = proc1->utime + proc1->stime + proc1->cutime + proc1->cstime;
            unsigned long total_proc_jiffies2 = proc2->utime + proc2->stime + proc2->cutime + proc2->cstime;
            unsigned long delta_proc_jiffies = total_proc_jiffies2 - total_proc_jiffies1;

            if(delta_system_jiffies > 0){
                res->cpu_percentage = ((long double)delta_proc_jiffies / (long double)delta_system_jiffies) * 100.0 * (long double)cores;
            } else {
                res->cpu_percentage = 0.0;
            }

            unsigned long total_mem_kb = sys_info2.total_mem_kb;

            if(total_mem_kb > 0){
                res->rss_percentage = ((long double)proc2->rss_kb / (long double)total_mem_kb) * 100.0;
                res->pss_percentage = ((long double)proc2->pss_kb / (long double)total_mem_kb) * 100.0;
            } else {
                res->rss_percentage = 0.0;
                res->pss_percentage = 0.0;
            }

            // Uptime: process runtime in seconds = system uptime - (starttime / HZ)
            long double proc_start_secs = (long double)proc2->starttime / (long double)ticks_per_second;
            long double proc_runtime = sys_info2.uptime - proc_start_secs;
            if (proc_runtime < 0.0L) proc_runtime = 0.0L;
            res->current_process_uptime = proc_runtime;

            // Copy data from proc2 into final_list entry
            res->rss_kb = proc2->rss_kb;
            res->pss_kb = proc2->pss_kb;
            res->pid = proc2->pid;
            res->ppid = proc2->ppid;
            res->starttime = proc2->starttime;
            strncpy(res->name, proc2->name, sizeof(res->name) - 1);
            res->name[sizeof(res->name) - 1] = '\0';
            
            calculated_count++;
        }
    }

    freeHashMap();
    
    if (final_count_ptr) {
        *final_count_ptr = calculated_count;
    }
    
    return final_list;
}




void print_cal_processes_to_csv(const ProcessResourceInfo *list, int count) {
    const char *filename = "resource.csv";
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening resource.csv for writing");
        return;
    }

    fprintf(file, "PID,PPID,Name,StartTime,CPU_%%,Uptime_Sec,RSS_KB,PSS_KB,RSS_%%,PSS_%%\n");

    for (int i = 0; i < count; i++) {
        const ProcessResourceInfo *res = &list[i];

        fprintf(file, "%d,%d,\"%s\",%ld,%.8Lf,%.2Lf,%lu,%lu,%.2Lf,%.2Lf\n",
            res->pid,
            res->ppid,
            res->name,
            res->starttime,
            res->cpu_percentage,
            res->current_process_uptime,
            res->rss_kb,
            res->pss_kb,
            res->rss_percentage,
            res->pss_percentage
        );
    }

    fclose(file);
    printf("✅ Individual process data written to %s.\n", filename);
}







