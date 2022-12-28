#ifndef PROC_STAT_UTILS_H
#define PROC_STAT_UTILS_H

#include <stdio.h>
#include <stdbool.h>

typedef struct {
#define PROCSTATCPUENTRY_CPU_NAME_SIZE 16 /* If you want to change this value make sure to edit the define below too */
#define PROCSTATCPUENTRY_CPU_NAME_SCANF_FORMAT_SPECIFIER "%15s"
    char cpu_name[PROCSTATCPUENTRY_CPU_NAME_SIZE];
    unsigned long user;
    unsigned long nice;
    unsigned long system;
    unsigned long idle;
    unsigned long iowait;
    unsigned long irq;
    unsigned long softirq;
    unsigned long steal;
    unsigned long guest;
    unsigned long guest_nice;
} ProcStatCpuEntry;

/*
 * Read and parse the contents of the /proc/stat file, and save CPU time stats to cpu_entries.
 * The first entry is the CPU average, the subsequent entries are for each CPU core/thread.
 *
 * Params
 *  proc_stat_file:
 *      Valid pointer to an fopen()'ed /proc/stat file.
 *      The position indicator must be set to the beginning of the file.
 *  max_cpu_entries:
 *      Maximum number of CPU entries to be read.
 *  cpu_entries:
 *      Array of size at least max_cpu_entries for storing the results.
 *
 * Returns
 *  On success: the total number of entries parsed (should be equal to the number of cores/threads + 1).
 *  On failure: -1.
 *
 * Will fail if max_cpu_entries is lower than the number of entries that happen to appear in the file.
 * cpu_entries might be modified even if the function fails.
 *
 * A successful call sets the position indicator of proc_stat_file to the beginning.
 */
int read_and_parse_proc_stat_file(FILE proc_stat_file[static 1], int max_cpu_entries, ProcStatCpuEntry cpu_entries[max_cpu_entries]);

/*
 * Calculate CPU usage based on the previous and current CPU time stats.
 * The results are saved to cpu_usage.
 *
 * Returns true on success and false on failure.
 *
 * Might fail if the CPU entries are filled with invalid data, such that the calculations would result in an arithmetic exception.
 */
bool calculate_cpu_usage(int n_cpu_entries, ProcStatCpuEntry previous_stats[n_cpu_entries], ProcStatCpuEntry current_stats[n_cpu_entries], double cpu_usage[n_cpu_entries]);

/*
 * Print CPU usage stored in the cpu_usage array.
 * If n_cpu_entries is less than 2 the function doesn't do anything.
 */
void print_cpu_usage(int n_cpu_entries, char cpu_names[n_cpu_entries][PROCSTATCPUENTRY_CPU_NAME_SIZE], double cpu_usage[n_cpu_entries]);

#endif /* PROC_STAT_UTILS_H */
