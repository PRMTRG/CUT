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

typedef struct {
    AnalyzerArgs *args;
    ProcStatCpuEntry *cpu_entries_arr[2];
    int n_cpu_entries_arr[2];
    int cpu_entries_next_index;
    int n_cpu_usage;
    float *cpu_usage;
    char (*cpu_names)[PROCSTATCPUENTRY_CPU_NAME_SIZE];
} AnalyzerPrivateState;

static struct {
    bool analyzer_initialized;
    int max_cpu_entries;
    ProcStatCpuEntry *submitted_cpu_entries;
    int n_submitted_cpu_entries;
    bool new_data_submitted;
} shared_state;

static pthread_mutex_t analyzer_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_analyzer_initialized = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_data_submitted = PTHREAD_COND_INITIALIZER;

static bool
analyzer_retrieve_submitted_data(AnalyzerPrivateState *private_state)
{
    bool succ;

    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &analyzer_lock);

    if (!shared_state.new_data_submitted) {
        // TODO: make this a timed wait
        pthread_cond_wait(&cond_on_data_submitted, &analyzer_lock);
    }

    if (!shared_state.new_data_submitted) {
        succ = false;
    } else {
        succ = true;

        shared_state.new_data_submitted = false;

        ProcStatCpuEntry *dest = private_state->cpu_entries_arr[private_state->cpu_entries_next_index];
        memcpy(dest, shared_state.submitted_cpu_entries, (size_t)shared_state.n_submitted_cpu_entries * sizeof(shared_state.submitted_cpu_entries[0]));
        private_state->n_cpu_entries_arr[private_state->cpu_entries_next_index] = shared_state.n_submitted_cpu_entries;

        private_state->cpu_entries_next_index ^= 1;
    }

    pthread_cleanup_pop(1);
    return succ;
}

static void
analyzer_process_data(AnalyzerPrivateState *private_state)
{
    if (private_state->n_cpu_entries_arr[0] != private_state->n_cpu_entries_arr[1]) {
        return;
    }

    ProcStatCpuEntry *previous = private_state->cpu_entries_arr[private_state->cpu_entries_next_index];
    ProcStatCpuEntry *current = private_state->cpu_entries_arr[private_state->cpu_entries_next_index ^ 1];

    bool bret = calculate_cpu_usage(private_state->n_cpu_entries_arr[0], previous, current, private_state->cpu_usage);
    assert(bret);

    private_state->n_cpu_usage = private_state->n_cpu_entries_arr[0];

    for (int i = 0; i < private_state->n_cpu_usage; i++) {
        memcpy(private_state->cpu_names[i], current[i].cpu_name, sizeof(current[0].cpu_name));
    }

    printer_submit_data(private_state->n_cpu_usage, private_state->cpu_names, private_state->cpu_usage);
}

static void
analyzer_deinit(void *arg)
{
    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);

    AnalyzerPrivateState *private_state = arg;

    free(private_state->args);
    free(private_state->cpu_entries_arr[0]);
    free(private_state->cpu_entries_arr[1]);
    free(private_state->cpu_usage);
    free(private_state->cpu_names);

    free(private_state);

    free(shared_state.submitted_cpu_entries);

    memset(&shared_state, 0, sizeof(shared_state));

    iret = pthread_mutex_unlock(&analyzer_lock);
    assert(iret == 0);
}

static AnalyzerPrivateState *
analyzer_init(void *arg)
{
    int iret = pthread_mutex_lock(&analyzer_lock);
    assert(iret == 0);

    AnalyzerPrivateState *private_state = ecalloc(1, sizeof(*private_state));
    memset(&shared_state, 0, sizeof(shared_state));

    private_state->args = arg;

    int max_cpu_entries = private_state->args->max_cpu_entries;

    private_state->cpu_entries_arr[0] = emalloc((size_t)max_cpu_entries * sizeof(private_state->cpu_entries_arr[0][0]));
    private_state->cpu_entries_arr[1] = emalloc((size_t)max_cpu_entries * sizeof(private_state->cpu_entries_arr[1][0]));
    private_state->cpu_usage = emalloc((size_t)max_cpu_entries * sizeof(private_state->cpu_usage[0]));
    private_state->cpu_names = emalloc((size_t)max_cpu_entries * sizeof(private_state->cpu_names[0]));

    shared_state.max_cpu_entries = max_cpu_entries;
    shared_state.submitted_cpu_entries = emalloc((size_t)max_cpu_entries * sizeof(shared_state.submitted_cpu_entries[0]));

    shared_state.analyzer_initialized = true;

    iret = pthread_cond_signal(&cond_on_analyzer_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&analyzer_lock);
    assert(iret == 0);

    return private_state;
}

static void
analyzer_loop(AnalyzerPrivateState *private_state)
{
    while (1) {
        bool did_retrieve_data = analyzer_retrieve_submitted_data(private_state);
        if (did_retrieve_data) {
            analyzer_process_data(private_state);
        }

        // TODO: signal to watchdog
    }
}

void *
analyzer_run(void *arg)
{
    AnalyzerPrivateState *private_state = analyzer_init(arg);

    pthread_cleanup_push(analyzer_deinit, private_state);

    analyzer_loop(private_state);

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

    ensure_initialized(&shared_state.analyzer_initialized, &cond_on_analyzer_initialized, &analyzer_lock);

    if (n_cpu_entries > shared_state.max_cpu_entries) {
        succ = false;
    } else {
        succ = true;
        memcpy(shared_state.submitted_cpu_entries, cpu_entries, (size_t)n_cpu_entries * sizeof(cpu_entries[0]));
        shared_state.n_submitted_cpu_entries = n_cpu_entries;
        shared_state.new_data_submitted = true;

        pthread_cond_signal(&cond_on_data_submitted);
    }

    pthread_cleanup_pop(1);
    return succ;
}
