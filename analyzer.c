#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "analyzer.h"
#include "utils.h"
#include "proc_stat_utils.h"

static pthread_mutex_t analyzer_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_analyzer_initialized = PTHREAD_COND_INITIALIZER;
static bool analyzer_initialized;

static int max_cpu_entries;

static pthread_cond_t cond_on_data_submitted = PTHREAD_COND_INITIALIZER;
static ProcStatCpuEntry *submitted_cpu_entries;
static int n_submitted_cpu_entries;
static bool new_data_submitted;

static ProcStatCpuEntry *cpu_entries_arr[2];
static int n_cpu_entries_arr[2];
static int cpu_entries_next_index;

static float *cpu_usage;
static int n_cpu_usage;

static void
cleanup_mutex_unlock(void *mutex)
{
    int iret = pthread_mutex_unlock(mutex);
    assert(iret == 0);
}

static bool
retrieve_submitted_data(void)
{
    bool succ = false;

    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &analyzer_lock);

    if (!new_data_submitted) {
        // TODO: make this a timed wait
        pthread_cond_wait(&cond_on_data_submitted, &analyzer_lock);
    }

    if (new_data_submitted) {
        succ = true;

        new_data_submitted = false;

        ProcStatCpuEntry *dest = cpu_entries_arr[cpu_entries_next_index];
        memcpy(dest, submitted_cpu_entries, (size_t)n_submitted_cpu_entries * sizeof(submitted_cpu_entries[0]));
        n_cpu_entries_arr[cpu_entries_next_index] = n_submitted_cpu_entries;

        cpu_entries_next_index ^= 1;
    }

    pthread_cleanup_pop(1);
    return succ;
}

static void
process_data(void)
{
    if (n_cpu_entries_arr[0] != n_cpu_entries_arr[1]) {
        return;
    }

    ProcStatCpuEntry *previous = cpu_entries_arr[cpu_entries_next_index];
    ProcStatCpuEntry *current = cpu_entries_arr[cpu_entries_next_index ^ 1];

    bool bret = calculate_cpu_usage(n_cpu_entries_arr[0], previous, current, cpu_usage);
    assert(bret);

    n_cpu_usage = n_cpu_entries_arr[0];

    // TODO: move printing to Printer thread (not yet added)
    print_cpu_usage(n_cpu_usage, current, cpu_usage);
}

void *
analyzer_run(void *arg)
{
    pthread_cleanup_push(free, arg);

    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);

    iret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    assert(iret == 0);

    AnalyzerArgs *args = arg;

    max_cpu_entries = args->max_cpu_entries;

    cpu_entries_arr[0] = emalloc((size_t)max_cpu_entries * sizeof(cpu_entries_arr[0][0]));
    pthread_cleanup_push(free, cpu_entries_arr[0]);
    cpu_entries_arr[1] = emalloc((size_t)max_cpu_entries * sizeof(cpu_entries_arr[1][0]));
    pthread_cleanup_push(free, cpu_entries_arr[1]);

    submitted_cpu_entries = emalloc((size_t)max_cpu_entries * sizeof(submitted_cpu_entries[0]));
    pthread_cleanup_push(free, submitted_cpu_entries);

    cpu_usage = emalloc((size_t)max_cpu_entries * sizeof(cpu_usage[0]));
    pthread_cleanup_push(free, cpu_usage);

    analyzer_initialized = true;

    iret = pthread_mutex_unlock(&analyzer_lock);
    assert(iret == 0);

    iret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    assert(iret == 0);

    while (1) {
        bool did_retrieve_data = retrieve_submitted_data();
        if (did_retrieve_data) {
            process_data();
        }

        // TODO: signal to watchdog
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
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

    while (!analyzer_initialized) {
        pthread_cond_wait(&cond_on_analyzer_initialized, &analyzer_lock);
    }

    iret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    assert(iret == 0);

    if (n_cpu_entries > max_cpu_entries) {
        succ = false;
    } else {
        succ = true;
        memcpy(submitted_cpu_entries, cpu_entries, (size_t)n_cpu_entries * sizeof(cpu_entries[0]));
        n_submitted_cpu_entries = n_cpu_entries;
        new_data_submitted = true;

        pthread_cond_signal(&cond_on_data_submitted);
    }

    iret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    assert(iret == 0);
    pthread_cleanup_pop(1);
    return succ;
}
