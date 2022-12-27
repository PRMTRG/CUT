#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

typedef struct {
    ReaderArgs args;
    FILE *proc_stat_file;
    bool first_sleep_done;
    ProcStatCpuEntry *cpu_entries;
} ReaderPrivateState;

static void
reader_deinit(void *arg)
{
    ReaderPrivateState *private_state = arg;

    if (private_state->proc_stat_file) {
        int iret = fclose(private_state->proc_stat_file);
        assert(iret == 0);
    }

    free(private_state->cpu_entries);

    free(private_state);
}

static ReaderPrivateState *
reader_init(void *arg)
{
    ReaderPrivateState *private_state = ecalloc(1, sizeof(*private_state));

    memcpy(&private_state->args, arg, sizeof(private_state->args));
    free(arg);

    private_state->proc_stat_file = fopen("/proc/stat", "r");
    if (!private_state->proc_stat_file) {
        elog("Failed to open /proc/stat");
        reader_deinit(private_state);
        pthread_exit(NULL);
    }

    private_state->cpu_entries = emalloc((size_t)private_state->args.max_cpu_entries * sizeof(private_state->cpu_entries[0]));

    return private_state;
}

static void
reader_loop(ReaderPrivateState *private_state)
{
    while (1) {
        int n_cpu_entries = read_and_parse_proc_stat_file(private_state->proc_stat_file,
                private_state->args.max_cpu_entries, private_state->cpu_entries);
        assert(n_cpu_entries > 1);

        bool bret = analyzer_submit_data(n_cpu_entries, private_state->cpu_entries);
        assert(bret);

        /* TODO: signal to watchdog */

        /* Reduce the duration of the first sleep to reduce program startup time */
        if (!private_state->first_sleep_done) {
            private_state->first_sleep_done = true;

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
}

void *
reader_run(void *arg)
{
    ReaderPrivateState *private_state = reader_init(arg);

    pthread_cleanup_push(reader_deinit, private_state);

    reader_loop(private_state);

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}
