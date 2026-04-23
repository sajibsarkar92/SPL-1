# SPL-1 — Linux System Monitor: Suggestions for Improvement

> A detailed report covering code quality improvements, efficiency optimizations, additional feature ideas, and UI/visualization strategies for the SPL-1 project.

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Code Quality & Efficiency Improvements](#2-code-quality--efficiency-improvements)
   - 2.1 [Build System](#21-add-a-build-system-makefile)
   - 2.2 [Memory Safety & Resource Management](#22-memory-safety--resource-management)
   - 2.3 [Thread Safety & Graceful Shutdown](#23-thread-safety--graceful-shutdown)
   - 2.4 [Data Structure Optimizations](#24-data-structure-optimizations)
   - 2.5 [Code Organization & Hygiene](#25-code-organization--hygiene)
   - 2.6 [Error Handling](#26-error-handling)
   - 2.7 [Clustering Algorithm](#27-clustering-algorithm)
   - 2.8 [I/O and File Handling](#28-io-and-file-handling)
3. [Suggested Additional Features](#3-suggested-additional-features)
4. [UI & Visualization Recommendations](#4-ui--visualization-recommendations)
   - 4.1 [Terminal-Based UI (TUI)](#41-terminal-based-ui-tui)
   - 4.2 [Web-Based Dashboard](#42-web-based-dashboard)
   - 4.3 [Specific Visualization Ideas](#43-specific-visualization-ideas)
5. [Summary of Quick Wins](#5-summary-of-quick-wins)

---

## 1. Project Overview

SPL-1 is a Linux system monitor written in C that:

- Collects real-time process data from `/proc` (CPU ticks, memory via RSS/PSS, UIDs, PIDs)
- Calculates per-process resource consumption using a sliding-window snapshot approach
- Aggregates data by application (root parent process)
- Clusters applications into `LOW_IMPACT`, `MEDIUM_IMPACT`, and `HIGH_IMPACT` categories using K-Medoids
- Provides an interactive CLI for process management (suspend, resume, terminate, kill, renice)
- Exports data to four CSV files: `raw_data.csv`, `resource.csv`, `aggregated_data.csv`, `clustered_report.csv`

**Architecture:**

```
main.c  →  core.c (background thread, 4-sec loop)
               ├── data_collector/data_extractor.c   → raw_data.csv, system_info.txt
               ├── data_collector/resource_calculator.c → resource.csv
               ├── data_collector/data_aggregator.c  → aggregated_data.csv
               └── cluster/clustering.c              → clustered_report.csv
           display/menu.c (foreground interactive menu)
               └── controller/process_control.c (SIGSTOP/SIGCONT/SIGTERM/SIGKILL/renice)
```

---

## 2. Code Quality & Efficiency Improvements

### 2.1 Add a Build System (Makefile)

**Current state:** No `Makefile` or `CMakeLists.txt` exists. The project contains pre-compiled binaries (`my_m1`, `my_monitor`, `my_program`) committed to the repo.

**Suggestion:** Add a minimal `Makefile` to automate the build and remove binaries from version control.

```makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS = -pthread -lm

SRCS = main.c core.c \
       data_collector/data_extractor.c \
       data_collector/resource_calculator.c \
       data_collector/data_aggregator.c \
       cluster/clustering.c \
       controller/process_control.c \
       display/menu.c

TARGET = system_monitor

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) *.csv system_info.txt
```

Also add a `.gitignore` to exclude compiled binaries and generated data files:

```
my_m1
my_monitor
my_program
system_monitor
*.csv
system_info.txt
*.o
```

**Impact:** Reproducible builds, easier onboarding, cleaner repository.

---

### 2.2 Memory Safety & Resource Management

#### a) Uninitialized `assignments` array in clustering (`cluster/clustering.c`, line 22)

```c
int *assignments = malloc(sizeof(int) * count);
```

This array is read on line 41 (`if (assignments[i] != best_cluster)`) before being written in the first iteration. Since `malloc` does not zero memory, this leads to **undefined behavior**.

**Fix:** Change `malloc` to `calloc`:

```c
int *assignments = calloc(count, sizeof(int));
```

#### b) Missing NULL check for `assignments` (`cluster/clustering.c`, line 22)

If `malloc`/`calloc` fails, the code proceeds to dereference a `NULL` pointer.

**Fix:** Add a null check immediately after allocation:

```c
int *assignments = calloc(count, sizeof(int));
if (!assignments) return;
```

#### c) Static global arrays in `data_aggregator.c` (lines 10–11)

```c
static int *root_cache = NULL;
static ProcessInfo **pid_lookup = NULL;
```

These are allocated once (`initialize_root_cache_and_lookup`) but **never freed**. Over long runtimes this is a persistent allocation based on `pid_max` (up to 4 MB+ for arrays sized by PID max).

**Fix:** Add a cleanup function and call it at program exit or at the end of each aggregation cycle:

```c
void cleanup_aggregator(void) {
    free(root_cache);  root_cache = NULL;
    free(pid_lookup);  pid_lookup = NULL;
}
```

#### d) Redundant `memset` in `data_aggregator.c` (line 206)

After `calculate_and_update_deltas` clears `pid_lookup` (line 231), `build_pid_lookup` is called again on the `curr_list`. But the `memset` on line 206 is done **inside** `calculate_and_update_deltas`, while the caller also does `build_pid_lookup` (line 291). This double-lookup build is fine but could be clarified with comments.

---

### 2.3 Thread Safety & Graceful Shutdown

#### a) No graceful shutdown mechanism (`core.c`, line 24)

```c
while (1) {  // Infinite loop with no exit condition
```

The monitor thread runs forever. When the user exits the menu, `main()` returns and the process terminates without canceling the thread.

**Fix:** Use a `volatile sig_atomic_t` flag to signal the thread to stop:

```c
// core.h
extern volatile sig_atomic_t monitor_running;

// core.c
volatile sig_atomic_t monitor_running = 1;

void *unified_monitor_thread_func(void *arg) {
    // ...
    while (monitor_running) {
        sleep(interval);
        // ...
    }
    return NULL;
}

// main.c — after run_interactive_menu() returns:
monitor_running = 0;
pthread_join(monitor_thread, NULL);
```

#### b) Mutex scope is too broad (`core.c`, lines 31–52)

The mutex `file_access_mutex` is held during all CSV writes **and** during the CPU-intensive resource calculation and clustering. This blocks the interactive menu from reading CSV data during the entire computation cycle.

**Fix:** Only lock around the file I/O operations, not the compute steps:

```c
// Compute outside the lock
ProcessResourceInfo *res_list = calculate_individual_resources(...);
AppSummary *summary_list = NULL;
int app_count = aggregate_live_data(...);

// Lock only for writing
pthread_mutex_lock(&file_access_mutex);
print_raw_data_to_csv(new_list, new_count);
write_system_info(new_sys);
if (res_list) print_cal_processes_to_csv(res_list, res_count);
if (summary_list) {
    print_aggregated_data_to_csv(summary_list, app_count);
    perform_clustering_and_export(summary_list, app_count);
}
pthread_mutex_unlock(&file_access_mutex);
```

---

### 2.4 Data Structure Optimizations

#### a) Linear search in `build_AppSummary_list` (`data_aggregator.c`, lines 157–161)

```c
for (int j = 0; j < count; j++) {
    if (temp_list[j].root_pid == root_pid) { ... }
}
```

For every process, a linear scan of the summary list is performed. With hundreds of apps, this is **O(n × m)**.

**Fix:** Use the existing `pid_lookup`-style array (indexed by root_pid) to map root PIDs to their summary index in O(1):

```c
int *root_to_summary_idx = calloc(current_max_pid, sizeof(int));
memset(root_to_summary_idx, -1, current_max_pid * sizeof(int));
// ...
int index = root_to_summary_idx[root_pid];
```

#### b) Hash map bucket count (`resource_calculator.c`, line 8)

```c
#define HASH_MAP_BUCKETS 100003
```

100,003 buckets is reasonable for typical Linux systems with ~1000 processes. This is fine. However, the hash map is rebuilt **every cycle** from scratch. Since the bucket array is `static`, clearing all 100,003 entries in `HashMapBuilder` (line 53) with a loop is slower than using `memset`:

```c
memset(hashTable, 0, sizeof(hashTable));
```

---

### 2.5 Code Organization & Hygiene

#### a) Duplicate function declaration (`data_collector.h`, lines 103–107 and 115–121)

`aggregate_live_data` is declared twice with identical signatures. Remove the duplicate.

#### b) Commented-out code throughout the codebase

- `data_extractor.c` lines 74–95: Commented-out `read_cpu_info` function
- `data_collector.h` lines 49–60: Commented-out `ProcessRaw` struct

**Fix:** Remove dead commented-out code. Use version control (git history) to recover old code if needed.

#### c) Unused declarations in `core.h` (lines 12–13)

```c
int aggregate_raw_to_csv(void);
int raw_data_to_csv(void);
```

These functions are declared but never defined or called anywhere.

**Fix:** Remove these unused declarations.

#### d) Include path inconsistency (`display/menu.c`, line 4)

```c
#include "controller/process_control.h"
```

This relative path works from the project root but is fragile. Consider standardizing include paths using `-I` flags in the build system.

#### e) Add `const` correctness

Many functions accept pointer parameters they don't modify (e.g., `print_raw_data_to_csv`, `print_aggregated_data_to_csv`). Mark these as `const` for safety:

```c
void print_raw_data_to_csv(const ProcessInfo *list, int count);
void print_aggregated_data_to_csv(const AppSummary *list, int count);
```

---

### 2.6 Error Handling

#### a) `extract_processes` returns `-1` and `count` through the same channel

The return value conflates "error" (`-1`) and "count" (`0` or positive). This is functional but could lead to confusion. Document the convention clearly or use a separate error out-parameter.

#### b) Silent failures in CSV writes

If `fopen` fails in any of the CSV-writing functions, the error is printed with `perror` but no return code propagates to the caller. The monitoring loop in `core.c` does not check if writes succeeded.

**Fix:** Return error codes from CSV write functions and log failures:

```c
int print_raw_data_to_csv(const ProcessInfo *list, int count) {
    FILE *file = fopen("raw_data.csv", "w");
    if (!file) { perror("raw_data.csv"); return -1; }
    // ...
    fclose(file);
    return 0;
}
```

#### c) No input validation in process control (`controller/process_control.c`)

The `menu.c` passes user-provided PIDs directly to `kill()`. A user could accidentally input PID `1` (init) or `-1` (all processes). Adding a basic sanity check would prevent catastrophic mistakes:

```c
void terminate_process(int pid) {
    if (pid <= 1) {
        fprintf(stderr, "Refusing to terminate PID %d (protected).\n", pid);
        return;
    }
    // ...
}
```

---

### 2.7 Clustering Algorithm

#### a) Fixed thresholds for cluster labeling (`clustering.c`, lines 78–81)

```c
if (cpu > 15.0 || mem > 15.0) labels[k] = "HIGH_IMPACT";
else if (cpu > 2.0 || mem > 2.0) labels[k] = "MEDIUM_IMPACT";
else labels[k] = "LOW_IMPACT";
```

These are hardcoded magic numbers. On systems with different workloads, these thresholds may not be meaningful.

**Fix:** Label clusters **relative to each other** instead of using absolute thresholds. Sort the three medoids by their combined impact score and assign LOW/MEDIUM/HIGH by rank:

```c
// Rank medoids by combined impact (cpu + mem)
double scores[K_CLUSTERS];
int order[K_CLUSTERS] = {0, 1, 2};
for (int k = 0; k < K_CLUSTERS; k++)
    scores[k] = apps[medoid_indices[k]].cpu_percentage + apps[medoid_indices[k]].mem_percentage;

// Simple sort of 3 elements
for (int i = 0; i < K_CLUSTERS - 1; i++)
    for (int j = i + 1; j < K_CLUSTERS; j++)
        if (scores[order[i]] > scores[order[j]]) {
            int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
        }

labels[order[0]] = "LOW_IMPACT";
labels[order[1]] = "MEDIUM_IMPACT";
labels[order[2]] = "HIGH_IMPACT";
```

#### b) Distance calculation uses raw values, not normalized

The comment on line 10 says "Normalize values" but no normalization is applied. If CPU ranges 0–100 and Memory ranges 0–5, CPU will dominate clustering.

**Fix:** Normalize both dimensions to `[0, 1]` before computing distance:

```c
// Find min/max for each dimension
double cpu_min, cpu_max, mem_min, mem_max;
// ... compute from data ...
// Normalize: (value - min) / (max - min)
```

---

### 2.8 I/O and File Handling

#### a) CSV files are overwritten every cycle

Every 4-second cycle, all four CSV files are truncated and rewritten. This means:
- No historical data is preserved
- A reader (e.g., a web dashboard) may get a partial file if it reads mid-write

**Fix (minimal):** Write to a temporary file and atomically rename:

```c
FILE *file = fopen("resource.csv.tmp", "w");
// ... write data ...
fclose(file);
rename("resource.csv.tmp", "resource.csv");
```

#### b) Hardcoded file paths

All CSV filenames are hardcoded strings scattered across multiple source files (`"raw_data.csv"`, `"resource.csv"`, etc.).

**Fix:** Define them in a central header:

```c
// config.h
#define RAW_DATA_CSV       "raw_data.csv"
#define RESOURCE_CSV       "resource.csv"
#define AGGREGATED_CSV     "aggregated_data.csv"
#define CLUSTERED_CSV      "clustered_report.csv"
#define SYSTEM_INFO_FILE   "system_info.txt"
```

---

## 3. Suggested Additional Features

### 3.1 Historical Data Logging

**What:** Append timestamped snapshots to a historical log file instead of overwriting every cycle.

```csv
Timestamp,Root_PID,App_Name,CPU_Percent,Mem_Percent,Cluster_Label
2026-02-18T06:30:00,1234,"firefox",12.5,8.3,HIGH_IMPACT
2026-02-18T06:30:04,1234,"firefox",11.2,8.1,HIGH_IMPACT
```

**Why:** Enables trend analysis, anomaly detection, and time-series visualizations.

**Minimal change:** Add a single function `append_to_history_csv()` called after each clustering cycle in `core.c`.

---

### 3.2 Configurable Monitoring Interval

**What:** Allow the user to set the monitoring interval at runtime (e.g., via a command-line argument).

**Minimal change:** Replace the hardcoded `4` in `main.c` with `argv[1]`:

```c
int interval = (argc > 1) ? atoi(argv[1]) : 4;
```

---

### 3.3 Alert System for High-Impact Processes

**What:** When a process crosses a resource threshold, log an alert or print a warning.

```
⚠️ ALERT: "chrome" (PID 2345) exceeds 20% CPU for 3 consecutive cycles
```

**Minimal change:** After clustering, iterate the `HIGH_IMPACT` cluster and print warnings.

---

### 3.4 Process Tree Visualization

**What:** Show the parent-child relationship tree in the terminal.

```
firefox (PID 1234, CPU: 12.5%, Mem: 8.3%)
├── Web Content (PID 1240, CPU: 5.1%, Mem: 3.2%)
├── Web Content (PID 1241, CPU: 4.2%, Mem: 2.8%)
└── GPU Process  (PID 1242, CPU: 3.2%, Mem: 2.3%)
```

**Minimal change:** Leverage the existing parent-child data in `data_aggregator.c` to build and print a tree.

---

### 3.5 Per-User Resource Summary

**What:** Aggregate resource usage per UID/user in addition to per-application.

**Minimal change:** Add a new aggregation pass in `data_aggregator.c` that groups by `uid` instead of root PID.

---

### 3.6 Network I/O Monitoring

**What:** Read `/proc/[pid]/net/dev` or `/proc/[pid]/io` to track per-process network and disk I/O.

**Why:** CPU and memory alone don't capture processes that are I/O-bound.

---

### 3.7 Export to JSON

**What:** In addition to CSV, offer JSON output for easier integration with web dashboards.

```json
{
  "timestamp": "2026-02-18T06:30:00Z",
  "apps": [
    { "name": "firefox", "pid": 1234, "cpu": 12.5, "mem": 8.3, "cluster": "HIGH_IMPACT" }
  ]
}
```

**Minimal change:** Add a `print_to_json()` function alongside the CSV writers.

---

## 4. UI & Visualization Recommendations

The project currently exports structured CSV data every 4 seconds. This data is ideal for building rich visualizations. Below are concrete recommendations for both terminal-based and web-based UIs.

---

### 4.1 Terminal-Based UI (TUI)

Use the **ncurses** library (already available on most Linux systems) to build a real-time dashboard directly in the terminal.

**Layout concept:**

```
┌─────────────────── System Monitor ───────────────────┐
│ CPU: ████████████░░░░░░░░ 58%    Mem: ██████░░░░ 35% │
├──────────────────────────────────────────────────────┤
│ Top Applications by CPU Usage                        │
│ ┌────────────┬───────┬────────┬────────┬───────────┐ │
│ │ App Name   │  PID  │ CPU %  │ Mem %  │  Cluster  │ │
│ ├────────────┼───────┼────────┼────────┼───────────┤ │
│ │ firefox    │ 1234  │ 12.50  │  8.30  │ HIGH      │ │
│ │ code       │ 5678  │  8.20  │  6.10  │ HIGH      │ │
│ │ slack      │ 9012  │  3.10  │  4.20  │ MEDIUM    │ │
│ │ terminal   │ 3456  │  0.50  │  0.30  │ LOW       │ │
│ └────────────┴───────┴────────┴────────┴───────────┘ │
├──────────────────────────────────────────────────────┤
│ Cluster Distribution   [LOW: 45] [MED: 12] [HIGH: 3]│
│ ░░░░░░░░░░░░░░░░░░████████░░░█████                   │
├──────────────────────────────────────────────────────┤
│ [S]uspend  [R]esume  [K]ill  [P]riority  [Q]uit     │
└──────────────────────────────────────────────────────┘
```

**Implementation approach:**

1. **Install ncurses:** `sudo apt install libncurses-dev`
2. **Replace `display/menu.c`** with an ncurses-based interface
3. **Read CSV data** in the display thread (using the existing `file_access_mutex`)
4. **Refresh every cycle** (4 seconds) using `ncurses` `refresh()` and `wrefresh()`

**Key ncurses features to use:**
- `newwin()` / `subwin()` for layout panels
- `mvwprintw()` for positioned text
- `wattron(COLOR_PAIR(...))` for color-coding by cluster
- `wgetch()` with `nodelay()` for non-blocking input

---

### 4.2 Web-Based Dashboard

For a more polished, shareable, and interactive experience, build a lightweight web dashboard that consumes the CSV/JSON output.

**Recommended stack:**

| Component | Technology | Why |
|-----------|-----------|-----|
| Backend | **Python Flask** or **Node.js Express** | Minimal setup, serves CSV/JSON data via REST API |
| Frontend | **HTML + Chart.js** or **D3.js** | Lightweight, no build step needed |
| Real-time updates | **Server-Sent Events (SSE)** or polling | Push new data every 4 seconds |

**Architecture:**

```
[C Monitor] → writes CSV files → [Python/Node API Server] → REST/SSE → [Browser Dashboard]
```

**Minimal Python backend example (Flask):**

```python
from flask import Flask, jsonify
import csv

app = Flask(__name__)

@app.route('/api/clustered')
def get_clustered():
    data = []
    with open('clustered_report.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    return jsonify(data)

@app.route('/api/aggregated')
def get_aggregated():
    data = []
    with open('aggregated_data.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append(row)
    return jsonify(data)

if __name__ == '__main__':
    app.run(port=5000)
```

---

### 4.3 Specific Visualization Ideas

Below are concrete chart types mapped to the data your project already produces.

#### 4.3.1 Cluster Distribution — Pie Chart or Donut Chart

**Data source:** `clustered_report.csv`

**What it shows:** Proportion of applications in each impact category.

```
        ┌──────────────┐
        │  LOW: 75%    │  ██████████████████████████
        │  MEDIUM: 18% │  ██████
        │  HIGH: 7%    │  ██
        └──────────────┘
```

**Chart.js implementation:**

```javascript
new Chart(ctx, {
    type: 'doughnut',
    data: {
        labels: ['LOW_IMPACT', 'MEDIUM_IMPACT', 'HIGH_IMPACT'],
        datasets: [{
            data: [lowCount, medCount, highCount],
            backgroundColor: ['#4caf50', '#ff9800', '#f44336']
        }]
    }
});
```

---

#### 4.3.2 Top N Applications — Horizontal Bar Chart

**Data source:** `aggregated_data.csv`

**What it shows:** Top 10 apps by CPU or memory usage.

```
firefox     ████████████████████  12.50%
code        ████████████████      8.20%
slack       ████████              3.10%
terminal    ██                    0.50%
```

**Chart.js implementation:**

```javascript
new Chart(ctx, {
    type: 'bar',
    data: {
        labels: appNames,
        datasets: [
            { label: 'CPU %', data: cpuValues, backgroundColor: '#2196f3' },
            { label: 'Mem %', data: memValues, backgroundColor: '#ff9800' }
        ]
    },
    options: { indexAxis: 'y' }  // Horizontal bars
});
```

---

#### 4.3.3 CPU vs Memory Scatter Plot with Cluster Colors

**Data source:** `clustered_report.csv`

**What it shows:** Each application as a dot. X-axis = CPU%, Y-axis = Mem%. Color = cluster.

This directly visualizes what the K-Medoids algorithm computed.

```
  Mem %
   │
10 │         ● firefox (HIGH)
   │
 5 │    ● slack (MED)        ● code (HIGH)
   │
 1 │ ● terminal (LOW)   ● cron (LOW)
   └──────────────────────────────── CPU %
       0        5       10       15
```

**Chart.js implementation:**

```javascript
new Chart(ctx, {
    type: 'scatter',
    data: {
        datasets: [
            {
                label: 'LOW_IMPACT',
                data: lowPoints,        // [{x: cpu, y: mem}, ...]
                backgroundColor: '#4caf50'
            },
            {
                label: 'MEDIUM_IMPACT',
                data: medPoints,
                backgroundColor: '#ff9800'
            },
            {
                label: 'HIGH_IMPACT',
                data: highPoints,
                backgroundColor: '#f44336',
                pointRadius: 8
            }
        ]
    },
    options: {
        scales: {
            x: { title: { display: true, text: 'CPU %' }},
            y: { title: { display: true, text: 'Memory %' }}
        }
    }
});
```

---

#### 4.3.4 Real-Time Line Chart (Time Series)

**Data source:** Historical log (see [Section 3.1](#31-historical-data-logging))

**What it shows:** CPU and memory trends over time for selected applications.

```
  CPU %
  15 │          ╱╲
     │         ╱  ╲        ╱╲
  10 │    ╱╲  ╱    ╲──────╱  ╲  firefox
     │   ╱  ╲╱
   5 │──╱                       code
     │
   0 └───────────────────────────── Time
     T0   T1   T2   T3   T4   T5
```

**Chart.js implementation:**

```javascript
new Chart(ctx, {
    type: 'line',
    data: {
        labels: timestamps,
        datasets: [{
            label: 'firefox CPU%',
            data: firefoxCpu,
            borderColor: '#f44336',
            fill: false,
            tension: 0.3
        }]
    },
    options: {
        animation: { duration: 0 },  // Disable for real-time performance
        scales: { x: { type: 'time' } }
    }
});
```

---

#### 4.3.5 Sortable Data Table

**Data source:** `aggregated_data.csv`

**What it shows:** Full application list with sortable columns.

| App Name | Root PID | Processes | PSS (KB) | CPU % | Mem % | Cluster |
|----------|----------|-----------|----------|-------|-------|---------|
| firefox | 1234 | 8 | 524,288 | 12.50 | 8.30 | HIGH |
| code | 5678 | 12 | 393,216 | 8.20 | 6.10 | HIGH |
| slack | 9012 | 5 | 262,144 | 3.10 | 4.20 | MEDIUM |

**Implementation:** Use a lightweight JS table library like **DataTables** or **AG Grid (community)**, or simply an HTML `<table>` with CSS styling and JavaScript sort handlers.

Color-code rows by cluster:
- 🟢 Green background for `LOW_IMPACT`
- 🟠 Orange background for `MEDIUM_IMPACT`
- 🔴 Red background for `HIGH_IMPACT`

---

#### 4.3.6 Treemap — Memory Footprint by Application

**Data source:** `aggregated_data.csv` (Total_PSS_KB field)

**What it shows:** Rectangles sized proportionally to memory usage, colored by cluster.

```
┌────────────────────────┬─────────────┐
│                        │             │
│      firefox           │    code     │
│      (524 MB)          │  (393 MB)   │
│                        │             │
├──────────┬─────────────┼─────────────┤
│  slack   │  terminal   │   others    │
│ (262 MB) │  (64 MB)    │  (128 MB)   │
└──────────┴─────────────┴─────────────┘
```

**Library:** Use **D3.js treemap** or **Chart.js treemap plugin**.

---

#### 4.3.7 Process Count Stacked Bar Chart

**Data source:** `aggregated_data.csv` (Process_Count field)

**What it shows:** Number of child processes per application, stacked by cluster.

Useful for identifying apps that spawn many sub-processes (e.g., Chrome, VS Code).

---

### 4.4 Recommended Dashboard Layout

Combine the above visualizations into a single-page dashboard:

```
┌──────────────────────────────────────────────────────┐
│  🖥️  SPL-1 System Monitor Dashboard                  │
│  Last updated: 2026-02-18 06:30:04   Interval: 4s   │
├────────────────────────┬─────────────────────────────┤
│                        │                             │
│   Cluster Pie Chart    │  CPU vs Mem Scatter Plot    │
│   (Section 4.3.1)      │  (Section 4.3.3)           │
│                        │                             │
├────────────────────────┴─────────────────────────────┤
│                                                      │
│   Top 10 Applications — Horizontal Bar Chart         │
│   (Section 4.3.2)                                    │
│                                                      │
├──────────────────────────────────────────────────────┤
│                                                      │
│   CPU & Memory Trends Over Time — Line Chart         │
│   (Section 4.3.4)                                    │
│                                                      │
├──────────────────────────────────────────────────────┤
│                                                      │
│   Full Application Table (sortable, filterable)      │
│   (Section 4.3.5)                                    │
│                                                      │
├──────────────────────────────────────────────────────┤
│                                                      │
│   Memory Treemap                                     │
│   (Section 4.3.6)                                    │
│                                                      │
└──────────────────────────────────────────────────────┘
```

**Auto-refresh:** Use JavaScript `setInterval` to poll the API every 4 seconds and update all charts:

```javascript
setInterval(async () => {
    const res = await fetch('/api/aggregated');
    const data = await res.json();
    updateAllCharts(data);
}, 4000);
```

---

## 5. Summary of Quick Wins

These are the **smallest changes with the highest impact**, ranked by effort:

| # | Change | File(s) | Effort | Impact |
|---|--------|---------|--------|--------|
| 1 | Add `Makefile` | New file | 5 min | Reproducible builds |
| 2 | Add `.gitignore` | New file | 2 min | Clean repository |
| 3 | Fix uninitialized `assignments` array | `clustering.c:22` | 1 line | Fix undefined behavior |
| 4 | Add NULL check for `assignments` | `clustering.c:23` | 2 lines | Prevent crash |
| 5 | Add graceful shutdown flag | `core.c`, `core.h`, `main.c` | ~10 lines | Clean thread termination |
| 6 | Atomic CSV writes (write + rename) | All CSV writers | ~4 lines each | Prevent partial reads |
| 7 | Remove duplicate `aggregate_live_data` declaration | `data_collector.h` | Delete 7 lines | Code hygiene |
| 8 | Remove unused declarations in `core.h` | `core.h` | Delete 2 lines | Code hygiene |
| 9 | Add PID validation in process control | `process_control.c` | ~5 lines | Prevent accidental `kill -1` |
| 10 | Use relative cluster labeling | `clustering.c` | ~15 lines | Better clustering accuracy |
| 11 | Centralize file path constants | New `config.h` | ~10 lines | Maintainability |
| 12 | Add `const` to read-only pointer params | Multiple headers | ~6 lines | Type safety |
| 13 | Normalize clustering features | `clustering.c` | ~15 lines | Fix CPU-biased clustering |
| 14 | Add command-line interval argument | `main.c` | 2 lines | Configurability |
| 15 | Add JSON export alongside CSV | New function | ~30 lines | Dashboard integration |

---

*This report is based on a static analysis of the SPL-1 codebase as of February 2026. No code changes were made — all items above are suggestions for the project maintainers to evaluate and implement as appropriate.*
