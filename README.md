# Pulse System Monitor

Pulse is a custom Linux process manager and monitoring dashboard written entirely in C. It continuously reads telemetry from the Linux `/proc` filesystem to track CPU and memory usage, aggregates related processes into grouped applications, and categorizes their system impact using a k-medoids clustering algorithm.

## Features

* Real-time Telemetry: Extracts live data directly from `/proc` for processes and system specs.
* Process Aggregation: Intelligently groups multi-threaded or multi-process applications together for a cleaner view.
* Smart Clustering: Uses a sliding window and K-medoids machine learning to categorize apps into HIGH, MEDIUM, and LOW impact clusters.
* Interactive Dashboard: Htop-inspired, centered ncurses terminal UI with progress bars, dynamic scaling, and live sorting.
* Resource Control: Built-in shortcuts to Kill, Force Kill, Suspend, Resume, and adjust the Priority (renice) of specific processes.
* Data Export: Dumps raw, aggregated, and clustered data to CSV files for external use.

## Prerequisites

To compile and run Pulse, you will need a Linux environment with the following installed:

* GCC (GNU Compiler Collection)
* Ncurses Library (for the terminal UI)
* Standard C libraries (pthread, math)

## Compilation

The project does not currently use a Makefile. To compile the entire project from source, run the following command in the project root directory:

```bash
gcc -Wall -g -o pulse \
    main.c \
    core.c \
    data_collector/data_extractor.c \
    data_collector/data_aggregator.c \
    data_collector/resource_calculator.c \
    display/menu.c \
    controller/process_control.c \
    cluster/clustering.c \
    -lncurses -lpthread -lm
```

## Usage

Start the program using the compiled binary (run as root if you need to adjust negative priority levels or kill system processes):

```bash
./pulse
```

### UI Controls

Once the dashboard is running, use the following keyboard shortcuts to interact with system processes:

* `k` : Kill - Sends a polite termination signal to a specified PID.
* `f` : Force - Sends a SIGKILL to immediately terminate a specified PID.
* `s` : Suspend - Pauses a running process.
* `r` : Resume - Wakes up a suspended process.
* `p` : Priority - Prompts for a PID and a new nice value (-20 to 19) to change execution priority.
* `q` : Quit - Exits the application and safely stops the core threads.

## Directory Structure

* `cluster/` : Contains the K-medoids clustering logic.
* `controller/` : Contains the process control functions for sending system signals.
* `data_collector/` : Logic for `/proc` parsing, data aggregation, and CPU tick calculation.
* `display/` : The ncurses-based graphical menu and UI.