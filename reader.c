#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "reader.h"
#include "analyzer.h"
#include "utils.h"
#include "proc_stat_utils.h"
#include "thread_utils.h"
#include "logger.h"

static bool first_sleep_done;

static void
deinit(void *arg)
{
    (void)(arg);

    first_sleep_done = false;
}

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

    pthread_cleanup_push(deinit, NULL);

    while (1) {
        int n_cpu_entries = read_and_parse_proc_stat_file(proc_stat_file, max_cpu_entries, cpu_entries);
        assert(n_cpu_entries > 1);

        bool bret = analyzer_submit_data(n_cpu_entries, cpu_entries);
        assert(bret);

        /* TODO: signal to watchdog */

        /* Reduce the duration of the first sleep to reduce program startup time */
        if (!first_sleep_done) {
            first_sleep_done = true;

            struct timespec ts = {0};
            /* 100 miliseconds */
            ts.tv_nsec = 100 * 1000 * 1000;
            nanosleep(&ts, NULL);
        } else {
            struct timespec ts = {0};
            /* 1 second */
            ts.tv_sec = 1;
            nanosleep(&ts, NULL);
        }
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}
