#ifndef PRINTER_H
#define PRINTER_H

#include "proc_stat_utils.h"

typedef struct {
    int max_cpu_entries;
    bool use_watchdog;
} PrinterArgs;

void * printer_run(void *arg);

void printer_submit_data(int n_cpu_entries, char cpu_names[n_cpu_entries][PROCSTATCPUENTRY_CPU_NAME_SIZE], double cpu_usage[n_cpu_entries]);

#endif /* PRINTER_H */
