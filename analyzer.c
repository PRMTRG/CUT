#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "analyzer.h"
#include "utils.h"
#include "proc_stat_utils.h"
#include "printer.h"
#include "thread_utils.h"
#include "watchdog.h"

typedef struct {
    AnalyzerArgs *args;
    ProcStatCpuEntry *cpu_entries_arr[2];
    int n_cpu_entries_arr[2];
    int cpu_entries_next_index;
    int n_cpu_usage;
    double *cpu_usage;
    char (*cpu_names)[PROCSTATCPUENTRY_CPU_NAME_SIZE];
} AnalyzerPrivateState;

static struct {
    bool analyzer_initialized;
    int max_cpu_entries;
    ProcStatCpuEntry *cpu_entries;
    int n_cpu_entries;
    bool new_data_submitted;
} shared;

static pthread_mutex_t analyzer_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_analyzer_initialized = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_data_submitted = PTHREAD_COND_INITIALIZER;

static bool
analyzer_retrieve_submitted_data(AnalyzerPrivateState *priv)
{
    bool succ;

    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &analyzer_lock);

    if (!shared.new_data_submitted) {
        cond_wait_seconds(&cond_on_data_submitted, &analyzer_lock, 1);
    }

    if (!shared.new_data_submitted) {
        succ = false;
    } else {
        succ = true;

        shared.new_data_submitted = false;

        ProcStatCpuEntry *dest = priv->cpu_entries_arr[priv->cpu_entries_next_index];
        memcpy(dest, shared.cpu_entries, (size_t)shared.n_cpu_entries * sizeof(shared.cpu_entries[0]));
        priv->n_cpu_entries_arr[priv->cpu_entries_next_index] = shared.n_cpu_entries;

        priv->cpu_entries_next_index ^= 1;
    }

    pthread_cleanup_pop(1);
    return succ;
}

static void
analyzer_process_data(AnalyzerPrivateState *priv)
{
    if (priv->n_cpu_entries_arr[0] != priv->n_cpu_entries_arr[1]) {
        return;
    }

    ProcStatCpuEntry *previous = priv->cpu_entries_arr[priv->cpu_entries_next_index];
    ProcStatCpuEntry *current = priv->cpu_entries_arr[priv->cpu_entries_next_index ^ 1];

    bool bret = calculate_cpu_usage(priv->n_cpu_entries_arr[0], previous, current, priv->cpu_usage);
    assert(bret);

    priv->n_cpu_usage = priv->n_cpu_entries_arr[0];

    for (int i = 0; i < priv->n_cpu_usage; i++) {
        memcpy(priv->cpu_names[i], current[i].cpu_name, sizeof(current[0].cpu_name));
    }

    printer_submit_data(priv->n_cpu_usage, priv->cpu_names, priv->cpu_usage);
}

static void
analyzer_deinit(void *arg)
{
    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);

    AnalyzerPrivateState *priv = arg;

    free(priv->args);
    free(priv->cpu_entries_arr[0]);
    free(priv->cpu_entries_arr[1]);
    free(priv->cpu_usage);
    free(priv->cpu_names);

    free(priv);

    free(shared.cpu_entries);

    memset(&shared, 0, sizeof(shared));

    iret = pthread_mutex_unlock(&analyzer_lock);
    assert(iret == 0);
}

static AnalyzerPrivateState *
analyzer_init(void *arg)
{
    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);

    AnalyzerPrivateState *priv = ecalloc(1, sizeof(*priv));
    memset(&shared, 0, sizeof(shared));

    priv->args = arg;

    int max_cpu_entries = priv->args->max_cpu_entries;

    priv->cpu_entries_arr[0] = emalloc((size_t)max_cpu_entries * sizeof(priv->cpu_entries_arr[0][0]));
    priv->cpu_entries_arr[1] = emalloc((size_t)max_cpu_entries * sizeof(priv->cpu_entries_arr[1][0]));
    priv->cpu_usage = emalloc((size_t)max_cpu_entries * sizeof(priv->cpu_usage[0]));
    priv->cpu_names = emalloc((size_t)max_cpu_entries * sizeof(priv->cpu_names[0]));

    shared.max_cpu_entries = max_cpu_entries;
    shared.cpu_entries = emalloc((size_t)max_cpu_entries * sizeof(shared.cpu_entries[0]));

    shared.analyzer_initialized = true;

    iret = pthread_cond_signal(&cond_on_analyzer_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&analyzer_lock);
    assert(iret == 0);

    return priv;
}

static void
analyzer_loop(AnalyzerPrivateState *priv)
{
    while (1) {
        bool did_retrieve_data = analyzer_retrieve_submitted_data(priv);
        if (did_retrieve_data) {
            analyzer_process_data(priv);
        }

        if (priv->args->use_watchdog) {
            watchdog_signal_active("Analyzer");
        }
    }
}

void *
analyzer_run(void *arg)
{
    assert(arg);

    AnalyzerPrivateState *priv = analyzer_init(arg);

    pthread_cleanup_push(analyzer_deinit, priv);

    analyzer_loop(priv);

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

bool
analyzer_submit_data(int n_cpu_entries, ProcStatCpuEntry cpu_entries[n_cpu_entries])
{
    bool succ;

    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &analyzer_lock);

    ensure_initialized(&shared.analyzer_initialized, &cond_on_analyzer_initialized, &analyzer_lock);

    if (n_cpu_entries > shared.max_cpu_entries) {
        succ = false;
    } else {
        succ = true;
        memcpy(shared.cpu_entries, cpu_entries, (size_t)n_cpu_entries * sizeof(cpu_entries[0]));
        shared.n_cpu_entries = n_cpu_entries;
        shared.new_data_submitted = true;

        pthread_cond_signal(&cond_on_data_submitted);
    }

    pthread_cleanup_pop(1);
    return succ;
}
