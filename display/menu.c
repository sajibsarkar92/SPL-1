
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include "menu.h"
#include "../controller/process_control.h"

// constants

#define MAX_ROWS 128
#define NAME_CAP 45
#define BAR_WIDTH 20
#define REFRESH_MS 1000

// fixed width
#define COL_PID 7
#define COL_CPU 7
#define COL_MEM 7
#define COL_STATUS 8

// color id
#define CP_HIGH 1
#define CP_MED 2
#define CP_LOW 3
#define CP_BAR 4

typedef struct
{
    char label[16];
    char name[64];
    int pid;
    double cpu;
    double mem;
} Row;

// load from csv

int load_rows(Row *rows, int cap)
{
    FILE *fp = fopen("clustered_report.csv", "r");
    if (!fp)
        return 0;

    char line[512];
    if (!fgets(line, sizeof(line), fp))
    {
        fclose(fp);
        return 0;
    }

    int n = 0;
    while (n < cap && fgets(line, sizeof(line), fp))
    {
        char *label = strtok(line, ",");
        char *name = strtok(NULL, ",");
        char *pid_str = strtok(NULL, ",");
        char *cpu_str = strtok(NULL, ",");
        char *mem_str = strtok(NULL, ",\n");
        if (!label || !name || !pid_str || !cpu_str || !mem_str)
            continue;

        strncpy(rows[n].label, label, sizeof(rows[n].label) - 1);
        rows[n].label[sizeof(rows[n].label) - 1] = '\0';
        strncpy(rows[n].name, name, sizeof(rows[n].name) - 1);
        rows[n].name[sizeof(rows[n].name) - 1] = '\0';
        rows[n].pid = atoi(pid_str);
        rows[n].cpu = atof(cpu_str);
        rows[n].mem = atof(mem_str);

        n++;
    }
    fclose(fp);
    return n;
}

int load_sysinfo(double *uptime_sec, unsigned long *total_mem_kb)
{
    *uptime_sec = 0;
    *total_mem_kb = 0;
    FILE *fp = fopen("system_info.txt", "r");
    if (!fp)
        return -1;
    char line[128];
    while (fgets(line, sizeof(line), fp))
    {
        sscanf(line, "Uptime (seconds): %lf", uptime_sec);
        sscanf(line, "Total Memory (KB): %lu", total_mem_kb);
    }
    fclose(fp);
    return 0;
}

// layout util

int calc_name_width(const Row *rows, int count)
{
    int w = 12; /* minimum so "Name" header has breathing room */
    for (int i = 0; i < count; i++)
    {
        int len = (int)strlen(rows[i].name);
        if (len > w)
            w = len;
    }
    return w > NAME_CAP ? NAME_CAP : w;
}

void draw_hline(int r, int x, chtype left, chtype fill, chtype junc, chtype right, const int *col_w, int n_cols)
{
    move(r, x);
    addch(left);
    for (int i = 0; i < n_cols; i++)
    {
        for (int j = 0; j < col_w[i]; j++)
            addch(fill);
        if (i < n_cols - 1)
            addch(junc);
    }
    addch(right);
}

void pad_to_border(int target_x)
{
    int cur = getcurx(stdscr);
    while (cur < target_x)
    {
        addch(' ');
        cur++;
    }
    addch(ACS_VLINE);
}

void draw_dashboard(const Row *rows, int count,
                    double uptime_sec, unsigned long total_mem_kb)
{
    erase();

    int name_w = calc_name_width(rows, count);

    int cw[5] = {
        COL_PID + 2,
        name_w + 2,
        COL_CPU + 2,
        COL_MEM + 2,
        COL_STATUS + 2};
    int inner = 0;
    for (int i = 0; i < 5; i++)
        inner += cw[i];
    inner += 4;

    int box_w = inner + 2;
    int ox = (COLS - box_w) / 2;
    if (ox < 0)
        ox = 0;
    int right_x = ox + box_w - 1;
    int row = 0;

    draw_hline(row, ox, ACS_ULCORNER, ACS_HLINE, ACS_HLINE, ACS_URCORNER, &inner, 1);
    row++;

    // ttitle
    {
        const char *title = "-- PULSE : Process Monitor --";
        int tlen = (int)strlen(title);
        int pad = inner - tlen;
        int pl = pad / 2;

        move(row, ox);
        addch(ACS_VLINE);
        for (int i = 0; i < pl; i++)
            addch(' ');
        attron(A_BOLD);
        printw("%s", title);
        attroff(A_BOLD);
        pad_to_border(right_x);
        row++;
    }

    // seperator
    draw_hline(row, ox, ACS_LTEE, ACS_HLINE, ACS_HLINE, ACS_RTEE, &inner, 1);
    row++;

    // cpu and meemory bar
    {
        double total_cpu = 0, total_mem = 0;
        for (int i = 0; i < count; i++)
        {
            total_cpu += rows[i].cpu;
            total_mem += rows[i].mem;
        }

        long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cores < 1)
            num_cores = 1;
        total_cpu /= num_cores;

        if (total_cpu > 100.0)
            total_cpu = 100.0;
        if (total_mem > 100.0)
            total_mem = 100.0;

        int up_h = (int)(uptime_sec / 3600);
        int up_m = (int)((uptime_sec - up_h * 3600) / 60);
        double mem_gb = (double)total_mem_kb / (1024.0 * 1024.0);

        /* CPU */
        {
            int filled = (int)(total_cpu / 100.0 * BAR_WIDTH);
            if (filled < 0)
                filled = 0;
            if (filled > BAR_WIDTH)
                filled = BAR_WIDTH;

            move(row, ox);
            addch(ACS_VLINE);
            printw("  CPU [");
            attron(COLOR_PAIR(CP_BAR) | A_BOLD);
            for (int i = 0; i < filled; i++)
                addch(ACS_CKBOARD);
            attroff(COLOR_PAIR(CP_BAR) | A_BOLD);
            for (int i = filled; i < BAR_WIDTH; i++)
                addch('-');
            printw("] %3d%%   Up: %dh %02dm", (int)total_cpu, up_h, up_m);
            pad_to_border(right_x);
            row++;
        }

        /* MEM */
        {
            int filled = (int)(total_mem / 100.0 * BAR_WIDTH);
            if (filled < 0)
                filled = 0;
            if (filled > BAR_WIDTH)
                filled = BAR_WIDTH;

            move(row, ox);
            addch(ACS_VLINE);
            printw("  MEM [");
            attron(COLOR_PAIR(CP_BAR) | A_BOLD);
            for (int i = 0; i < filled; i++)
                addch(ACS_CKBOARD);
            attroff(COLOR_PAIR(CP_BAR) | A_BOLD);
            for (int i = filled; i < BAR_WIDTH; i++)
                addch('-');
            printw("] %3d%%   %.1f GB", (int)total_mem, mem_gb);
            pad_to_border(right_x);
            row++;
        }
    }

    // table top bordr
    draw_hline(row, ox, ACS_LTEE, ACS_HLINE, ACS_TTEE, ACS_RTEE, cw, 5);
    row++;

    // table header
    {
        const char *hdr[] = {"PID", "Name", "CPU %", "MEM %", "Status"};
        int cwidth[] = {COL_PID, name_w, COL_CPU, COL_MEM, COL_STATUS};

        move(row, ox);
        addch(ACS_VLINE);
        for (int i = 0; i < 5; i++)
        {
            attron(A_BOLD | A_UNDERLINE);
            printw(" %-*s ", cwidth[i], hdr[i]);
            attroff(A_BOLD | A_UNDERLINE);
            if (i < 4)
                addch(ACS_VLINE);
        }
        addch(ACS_VLINE);
        row++;
    }

    /* ── header/data separator ── */
    draw_hline(row, ox, ACS_LTEE, ACS_HLINE, ACS_PLUS, ACS_RTEE, cw, 5);
    row++;

    /* ── data rows ── */
    int max_rows = LINES - row - 3;
    if (max_rows < 1)
        max_rows = 1;

    if (count == 0)
    {
        move(row, ox);
        addch(ACS_VLINE);
        printw("  Waiting for data...");
        pad_to_border(right_x);
        row++;
    }
    else
    {
        int show = count < max_rows ? count : max_rows;
        for (int i = 0; i < show; i++)
        {
            /* truncate long names */
            char dname[NAME_CAP + 1];
            if ((int)strlen(rows[i].name) > name_w)
            {
                strncpy(dname, rows[i].name, name_w - 2);
                dname[name_w - 2] = '\0';
                strcat(dname, "..");
            }
            else
            {
                strncpy(dname, rows[i].name, sizeof(dname) - 1);
                dname[sizeof(dname) - 1] = '\0';
            }

            /* pick color */
            int cp = CP_LOW;
            if (strstr(rows[i].label, "HIGH"))
                cp = CP_HIGH;
            else if (strstr(rows[i].label, "MEDIUM"))
                cp = CP_MED;

            /* strip _IMPACT from label for display */
            char slabel[16];
            strncpy(slabel, rows[i].label, sizeof(slabel) - 1);
            slabel[sizeof(slabel) - 1] = '\0';
            {
                char *p = strstr(slabel, "_IMPACT");
                if (p)
                    *p = '\0';
            }

            move(row, ox);
            addch(ACS_VLINE);

            /* PID — plain white */
            printw(" %-*d ", COL_PID, rows[i].pid);
            addch(ACS_VLINE);

            /* Name */
            attron(COLOR_PAIR(cp));
            printw(" %-*s ", name_w, dname);
            attroff(COLOR_PAIR(cp));
            addch(ACS_VLINE);

            /* CPU % */
            attron(COLOR_PAIR(cp));
            printw(" %*.2f ", COL_CPU - 1, rows[i].cpu);
            attroff(COLOR_PAIR(cp));
            addch(ACS_VLINE);

            /* MEM % */
            attron(COLOR_PAIR(cp));
            printw(" %*.2f ", COL_MEM - 1, rows[i].mem);
            attroff(COLOR_PAIR(cp));
            addch(ACS_VLINE);

            /* Status */
            attron(COLOR_PAIR(cp) | A_BOLD);
            printw(" %-*s ", COL_STATUS, slabel);
            attroff(COLOR_PAIR(cp) | A_BOLD);
            addch(ACS_VLINE);

            row++;
        }
    }

    /* ── table bottom border ── */
    draw_hline(row, ox, ACS_LTEE, ACS_HLINE, ACS_BTEE, ACS_RTEE, cw, 5);
    row++;

    /* ── shortcuts bar ── */
    {
        const char *keys = "[K]ill  [F]orce  [S]uspend  [R]esume  [P]riority  [Q]uit";
        int klen = (int)strlen(keys);
        int pad = inner - klen;
        int pl = pad / 2;

        move(row, ox);
        addch(ACS_VLINE);
        for (int i = 0; i < pl; i++)
            addch(' ');
        printw("%s", keys);
        pad_to_border(right_x);
        row++;
    }

    /* ── bottom border ── */
    draw_hline(row, ox, ACS_LLCORNER, ACS_HLINE, ACS_HLINE, ACS_LRCORNER, &inner, 1);

    refresh();
}

/* ── input prompts ── */

int get_pid_input(const char *action)
{
      /* block until user types – stop the refresh loop */
    echo();
    curs_set(1);
    timeout(-1); 

    int pw = 40;
    int px = (COLS - pw) / 2;
    if (px < 0)
        px = 0;
    int pr = LINES / 2;

    /* top border */
    move(pr - 1, px);
    addch(ACS_ULCORNER);
    for (int i = 0; i < pw - 2; i++)
        addch(ACS_HLINE);
    addch(ACS_URCORNER);

    /* prompt line */
    move(pr, px);
    addch(ACS_VLINE);
    printw(" %s | Enter PID: ", action);
    {
        int cur = getcurx(stdscr);
        int target = px + pw - 1;
        while (cur < target)
        {
            addch(' ');
            cur++;
        }
        addch(ACS_VLINE);
    }

    /* bottom border */
    move(pr + 1, px);
    addch(ACS_LLCORNER);
    for (int i = 0; i < pw - 2; i++)
        addch(ACS_HLINE);
    addch(ACS_LRCORNER);

    /* position cursor for input */
    int input_x = px + (int)strlen(action) + 16;
    move(pr, input_x);
    refresh();

    char buf[16];
    getstr(buf);


    timeout(REFRESH_MS); 
    noecho();
    curs_set(0);
    /* re-enable auto-refresh */
    return atoi(buf);
}

void get_priority_input(void)
{
       /* block until user types */
    echo();
    curs_set(1);
    timeout(-1);

    int pw = 44;
    int px = (COLS - pw) / 2;
    if (px < 0)
        px = 0;
    int pr = LINES / 2;

    // top
    move(pr - 1, px);
    addch(ACS_ULCORNER);
    for (int i = 0; i < pw - 2; i++)
        addch(ACS_HLINE);
    addch(ACS_URCORNER);

    // PID
    move(pr, px);
    addch(ACS_VLINE);
    printw(" PRIORITY | Enter PID:       ");
    {
        int c = getcurx(stdscr);
        while (c < px + pw - 1)
        {
            addch(' ');
            c++;
        }
        addch(ACS_VLINE);
    }

    // nice line
    move(pr + 1, px);
    addch(ACS_VLINE);
    printw("           Nice (-20..19):    ");
    {
        int c = getcurx(stdscr);
        while (c < px + pw - 1)
        {
            addch(' ');
            c++;
        }
        addch(ACS_VLINE);
    }

    // bottom line
    move(pr + 2, px);
    addch(ACS_LLCORNER);
    for (int i = 0; i < pw - 2; i++)
        addch(ACS_HLINE);
    addch(ACS_LRCORNER);

    // get PID
    move(pr, px + 24);
    refresh();
    char pid_buf[16];
    getstr(pid_buf);
    int pid = atoi(pid_buf);

    // gtting nice value
    move(pr + 1, px + 30);
    char nice_buf[16];
    getstr(nice_buf);
    int nice_val = atoi(nice_buf);

    timeout(REFRESH_MS); 

    noecho();
    curs_set(0);
    /* re-enable auto-refresh */

    if (pid > 0)
        renice_process(pid, nice_val);
}

// sorting helper

static int compare_rows_cpu_desc(const void *a, const void *b)
{
    const Row *ra = (const Row *)a;
    const Row *rb = (const Row *)b;
    if (rb->cpu > ra->cpu)
        return 1;
    if (rb->cpu < ra->cpu)
        return -1;
    return 0;
}

// entry point

void run_interactive_menu(void)
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(REFRESH_MS);

    if (has_colors())
    {
        start_color();
        use_default_colors();
        init_pair(CP_HIGH, COLOR_RED, -1);
        init_pair(CP_MED, COLOR_YELLOW, -1);
        init_pair(CP_LOW, COLOR_GREEN, -1);
        init_pair(CP_BAR, COLOR_CYAN, -1);
    }

    Row rows[MAX_ROWS];

    while (1)
    {
        int count = load_rows(rows, MAX_ROWS);
        if (count > 1)
        {
            qsort(rows, count, sizeof(Row), compare_rows_cpu_desc);
        }

        double uptime;
        unsigned long memkb;
        load_sysinfo(&uptime, &memkb);

        draw_dashboard(rows, count, uptime, memkb);

        int ch = getch();
        if (ch == ERR)
            continue;
        ch = tolower(ch);

        if (ch == 'q')
            break;

        if (ch == 'k')
        {
            int pid = get_pid_input("KILL");
            if (pid > 0)
                terminate_process(pid);
        }
        else if (ch == 'f')
        {
            int pid = get_pid_input("FORCE KILL");
            if (pid > 0)
                force_kill(pid);
        }
        else if (ch == 's')
        {
            int pid = get_pid_input("SUSPEND");
            if (pid > 0)
                suspend_process(pid);
        }
        else if (ch == 'r')
        {
            int pid = get_pid_input("RESUME");
            if (pid > 0)
                resume_process(pid);
        }
        else if (ch == 'p')
        {
            get_priority_input();
        }
    }

    endwin();
}

void display_main_menu(void)
{
    run_interactive_menu();
}