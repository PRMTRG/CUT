#include <stdio.h>
#include <string.h>
#include <tgmath.h>

#include "proc_stat_utils.h"
#include "utils.h"
#include "logger.h"

int
read_and_parse_proc_stat_file(FILE proc_stat_file[static 1], int max_cpu_entries, ProcStatCpuEntry cpu_entries[max_cpu_entries])
{
    int n_cpu_entries = 0;

    char buf[256];
    while (fgets(buf, sizeof(buf), proc_stat_file)) {
        if (strncmp(buf, "cpu", 3) != 0) {
            continue;
        }

        if (n_cpu_entries >= max_cpu_entries) {
            ELOG(true, "Exceeded max_cpu_entries");
            return -1;
        }

        ProcStatCpuEntry *ce = &cpu_entries[n_cpu_entries];

        int ret = sscanf(buf, PROCSTATCPUENTRY_CPU_NAME_SCANF_FORMAT_SPECIFIER " "
                "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                ce->cpu_name, &ce->user, &ce->nice, &ce->system, &ce->idle, &ce->iowait,
                &ce->irq, &ce->softirq, &ce->steal, &ce->guest, &ce->guest_nice);
        if (ret != 11) {
            ELOG(true, "Parsing CPU entry failed");
            return -1;
        }

        n_cpu_entries++;
    }
    if (ferror(proc_stat_file)) {
        ELOG(true, "IO error");
        return -1;
    }

    int iret = fseek(proc_stat_file, 0, SEEK_SET);
    if (iret != 0) {
        ELOG(true, "IO error");
        return -1;
    }

    return n_cpu_entries;
}

bool
calculate_cpu_usage(int n_cpu_entries, ProcStatCpuEntry previous_stats[n_cpu_entries], ProcStatCpuEntry current_stats[n_cpu_entries], double cpu_usage[n_cpu_entries])
{
    for (int i = 0; i < n_cpu_entries; i++) {
        ProcStatCpuEntry *prev = &previous_stats[i];
        ProcStatCpuEntry *curr = &current_stats[i];

        /*
         * Based on https://stackoverflow.com/a/23376195
         */
        unsigned long prev_idle = prev->idle + prev->iowait;
        unsigned long curr_idle = curr->idle + curr->iowait;

        unsigned long prev_non_idle = prev->user + prev->nice + prev->system + prev->irq + prev->softirq + prev->steal;
        unsigned long curr_non_idle = curr->user + curr->nice + curr->system + curr->irq + curr->softirq + curr->steal;

        unsigned long prev_total = prev_idle + prev_non_idle;
        unsigned long curr_total = curr_idle + curr_non_idle;

        unsigned long total_d = curr_total - prev_total;
        unsigned long idle_d = curr_idle - prev_idle;

        if (total_d == 0) {
            return false;
        }

        cpu_usage[i] = (double)(total_d - idle_d) / (double)total_d * 100;
    }

    return true;
}

static void
print_usage_bar(double percentage)
{
    percentage /= 5;
    int n_filled = (int)lround(percentage);

    printf("[");
    for (int i = 0; i < n_filled; i++) {
        printf("|");
    }
    for (int i = n_filled; i < 20; i++) {
        printf(" ");
    }
    printf("]");
}

void
print_cpu_usage(int n_cpu_entries, char cpu_names[n_cpu_entries][PROCSTATCPUENTRY_CPU_NAME_SIZE], double cpu_usage[n_cpu_entries])
{
    if (n_cpu_entries < 2) {
        return;
    }

    /* Clear the terminal */
    printf("\033[H\033[J");

    printf("Avg.\t");
    print_usage_bar(cpu_usage[0]);
    printf(" %5.1f%%\n", cpu_usage[0]);

    int n_cols = 2;
    int col_cnt = 0;
    for (int i = 1; i < n_cpu_entries; i++, col_cnt++) {
        printf("%s\t", cpu_names[i]);
        print_usage_bar(cpu_usage[i]);
        printf(" %5.1f%%", cpu_usage[i]);
        (col_cnt + 1) % n_cols == 0 ? printf("\n") : printf("\t\t");
    }
    if ((col_cnt + 1) % n_cols == 0) {
        printf("\n");
    }
}
