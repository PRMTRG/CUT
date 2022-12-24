#ifndef ANALYZER_H
#define ANALYZER_H

#include "proc_stat_utils.h"

typedef struct {
    int max_cpu_entries;
} AnalyzerArgs;

void * analyzer_run(void *arg);

bool analyzer_submit_data(int n_cpu_entries, ProcStatCpuEntry cpu_entries[n_cpu_entries]);

#endif /* ANALYZER_H */
