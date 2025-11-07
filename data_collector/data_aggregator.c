#include "data_collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Aggregate info per application name */
typedef struct {
    char name[128];
    unsigned long total_rss_kb;
    unsigned long total_utime;
    unsigned long total_stime;
    unsigned long count;
} AggEntry;

/* Read raw_data.csv (format matching print_raw_data_to_csv) and write aggregate.csv */
int aggregate_raw_to_csv(void) {
    const char *infile = "raw_data.csv";
    const char *outfile = "aggregate.csv";
    FILE *fin = fopen(infile, "r");
    if (!fin) return -1;

    char line[1024];
    /* skip header line */
    if (!fgets(line, sizeof(line), fin)) {
        fclose(fin);
        return -1;
    }

    AggEntry *entries = NULL;
    size_t cap = 0, used = 0;

    while (fgets(line, sizeof(line), fin)) {
        /* expected CSV: PID,"NAME","COMM",PPID,UTIME,STIME,CUTIME,CSTIME,STARTTIME,VmRSS_KB */
        int pid = 0, ppid = 0;
        long utime = 0, stime = 0, cutime = 0, cstime = 0, starttime = 0, vmrss = 0;
        char name[128] = {0};
        char comm[128] = {0};

        int scanned = sscanf(line,
            "%d,\"%127[^\"]\",\"%127[^\"]\",%d,%ld,%ld,%ld,%ld,%ld,%ld",
            &pid, name, comm, &ppid, &utime, &stime, &cutime, &cstime, &starttime, &vmrss);

        if (scanned < 10) {
            /* Try fallback: some lines may not strictly match, skip them */
            continue;
        }

        /* find or add entry by name */
        size_t idx = 0;
        for (; idx < used; ++idx) {
            if (strcmp(entries[idx].name, name) == 0) break;
        }
        if (idx == used) {
            /* new entry */
            if (used >= cap) {
                size_t new_cap = cap == 0 ? 64 : cap * 2;
                AggEntry *tmp = realloc(entries, new_cap * sizeof(AggEntry));
                if (!tmp) { free(entries); fclose(fin); return -1; }
                entries = tmp; cap = new_cap;
            }
            strncpy(entries[used].name, name, sizeof(entries[used].name)-1);
            entries[used].name[sizeof(entries[used].name)-1] = '\0';
            entries[used].total_rss_kb = 0;
            entries[used].total_utime = 0;
            entries[used].total_stime = 0;
            entries[used].count = 0;
            idx = used++;
        }

        entries[idx].total_rss_kb += (vmrss > 0 ? (unsigned long)vmrss : 0UL);
        entries[idx].total_utime += (utime > 0 ? (unsigned long)utime : 0UL);
        entries[idx].total_stime += (stime > 0 ? (unsigned long)stime : 0UL);
        entries[idx].count += 1;
    }

    fclose(fin);

    /* Write aggregate CSV */
    FILE *fout = fopen(outfile, "w");
    if (!fout) { free(entries); return -1; }

    fprintf(fout, "Name,ProcessCount,TotalRSS_KB,AvgRSS_KB,TotalUTIME,TotalSTIME\n");
    for (size_t i = 0; i < used; ++i) {
        unsigned long avg = entries[i].count ? (entries[i].total_rss_kb / entries[i].count) : 0;
        fprintf(fout, "\"%s\",%lu,%lu,%lu,%lu,%lu\n",
            entries[i].name,
            entries[i].count,
            entries[i].total_rss_kb,
            avg,
            entries[i].total_utime,
            entries[i].total_stime);
    }

    fclose(fout);
    free(entries);
    return 0;
}
