#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "printer.h"
#include "utils.h"
#include "proc_stat_utils.h"

static pthread_mutex_t printer_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_printer_initialized = PTHREAD_COND_INITIALIZER;
static bool printer_initialized;

static int max_cpu_entries;

static pthread_cond_t cond_on_data_submitted = PTHREAD_COND_INITIALIZER;
static int n_submitted_cpu_entries;
static char (*submitted_cpu_names)[PROCSTATCPUENTRY_CPU_NAME_SIZE];
static float *submitted_cpu_usage;
static bool new_data_submitted;

static void
cleanup_mutex_unlock(void *mutex)
{
    int iret = pthread_mutex_unlock(mutex);
    assert(iret == 0);
}

static void
print_usage(void)
{
    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    if (!new_data_submitted) {
        // TODO: Make this a timed wait
        pthread_cond_wait(&cond_on_data_submitted, &printer_lock);
    }

    if (new_data_submitted) {
        new_data_submitted = false;
        print_cpu_usage(n_submitted_cpu_entries, submitted_cpu_names, submitted_cpu_usage);
    }

    pthread_cleanup_pop(1);
}

void *
printer_run(void *arg)
{
    pthread_cleanup_push(free, arg);

    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);

    PrinterArgs *args = arg;
    max_cpu_entries = args->max_cpu_entries;

    submitted_cpu_names = emalloc((size_t)max_cpu_entries * sizeof(submitted_cpu_names[0]));
    pthread_cleanup_push(free, submitted_cpu_names);

    submitted_cpu_usage = emalloc((size_t)max_cpu_entries * sizeof(submitted_cpu_usage[0]));
    pthread_cleanup_push(free, submitted_cpu_usage);

    printer_initialized = true;
    iret = pthread_cond_signal(&cond_on_printer_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&printer_lock);
    assert(iret == 0);

    while (1) {
        print_usage();

        // TODO: signal to watchdog
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

void
printer_submit_data(int n_cpu_entries, char cpu_names[n_cpu_entries][PROCSTATCPUENTRY_CPU_NAME_SIZE], float cpu_usage[n_cpu_entries])
{
    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    while (!printer_initialized) {
        pthread_cond_wait(&cond_on_printer_initialized, &printer_lock);
    }

    if (n_cpu_entries > max_cpu_entries) {
        eprint("Exceeded max_cpu_entries");
    } else {
        memcpy(submitted_cpu_names, cpu_names, (size_t)n_cpu_entries * sizeof(cpu_names[0]));
        memcpy(submitted_cpu_usage, cpu_usage, (size_t)n_cpu_entries * sizeof(cpu_usage[0]));
        n_submitted_cpu_entries = n_cpu_entries;
        new_data_submitted = true;

        pthread_cond_signal(&cond_on_data_submitted);
    }

    pthread_cleanup_pop(1);
}
