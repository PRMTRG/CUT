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
    ReaderArgs *args;
    FILE *proc_stat_file;
    bool first_sleep_done;
    ProcStatCpuEntry *cpu_entries;
} ReaderPrivateState;

static void
reader_deinit(void *arg)
{
    ReaderPrivateState *priv = arg;

    free(priv->args);

    if (priv->proc_stat_file) {
        int iret = fclose(priv->proc_stat_file);
        assert(iret == 0);
    }

    free(priv->cpu_entries);

    free(priv);
}

static ReaderPrivateState *
reader_init(void *arg)
{
    ReaderPrivateState *priv = ecalloc(1, sizeof(*priv));

    priv->args = arg;

    priv->proc_stat_file = fopen("/proc/stat", "r");
    if (!priv->proc_stat_file) {
        elog(false, "Failed to open /proc/stat");
        reader_deinit(priv);
        pthread_exit(NULL);
    }

    priv->cpu_entries = emalloc((size_t)priv->args->max_cpu_entries * sizeof(priv->cpu_entries[0]));

    return priv;
}

static void
reader_loop(ReaderPrivateState *priv)
{
    while (1) {
        int n_cpu_entries = read_and_parse_proc_stat_file(priv->proc_stat_file,
                priv->args->max_cpu_entries, priv->cpu_entries);
        assert(n_cpu_entries > 1);

        bool bret = analyzer_submit_data(n_cpu_entries, priv->cpu_entries);
        assert(bret);

        /* TODO: signal to watchdog */

        /* Reduce the duration of the first sleep to reduce program startup time */
        if (!priv->first_sleep_done) {
            priv->first_sleep_done = true;

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
    ReaderPrivateState *priv = reader_init(arg);

    pthread_cleanup_push(reader_deinit, priv);

    reader_loop(priv);

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}
