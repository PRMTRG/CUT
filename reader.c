#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "reader.h"
#include "analyzer.h"
#include "utils.h"
#include "proc_stat_utils.h"
#include "thread_utils.h"
#include "logger.h"

void *
reader_run(void *arg)
{
    pthread_cleanup_push(free, arg);

    ReaderArgs *reader_args = arg;
    const int max_cpu_entries = reader_args->max_cpu_entries;

    FILE *proc_stat_file = fopen("/proc/stat", "r");
    if (!proc_stat_file) {
        elog("Failed to open /proc/stat");
        pthread_exit(NULL);
    }
    pthread_cleanup_push(cleanup_fclose, proc_stat_file);

    ProcStatCpuEntry *cpu_entries = emalloc((size_t)max_cpu_entries * sizeof(cpu_entries[0]));
    pthread_cleanup_push(free, cpu_entries);


    while (1) {
        int n_cpu_entries = read_and_parse_proc_stat_file(proc_stat_file, max_cpu_entries, cpu_entries);
        assert(n_cpu_entries > 1);

        bool bret = analyzer_submit_data(n_cpu_entries, cpu_entries);
        assert(bret);

        /* TODO: signal to watchdog */

        sleep(1);
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}
