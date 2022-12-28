#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "printer.h"
#include "utils.h"
#include "proc_stat_utils.h"
#include "thread_utils.h"
#include "logger.h"
#include "watchdog.h"

typedef struct {
    PrinterArgs *args;
} PrinterPrivateState;

static struct {
    PrinterArgs *args;
    bool printer_initialized;
    int max_cpu_entries;
    int n_cpu_entries;
    char (*cpu_names)[PROCSTATCPUENTRY_CPU_NAME_SIZE];
    double *cpu_usage;
    bool new_data_submitted;
} shared;

static pthread_mutex_t printer_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_printer_initialized = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_data_submitted = PTHREAD_COND_INITIALIZER;

static void
printer_print_usage(void)
{
    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    if (!shared.new_data_submitted) {
        cond_wait_seconds(&cond_on_data_submitted, &printer_lock, 1);
    }

    if (shared.new_data_submitted) {
        shared.new_data_submitted = false;
        print_cpu_usage(shared.n_cpu_entries, shared.cpu_names, shared.cpu_usage);
    }

    pthread_cleanup_pop(1);
}

static void
printer_deinit(void *arg)
{
    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    PrinterPrivateState *priv = arg;

    free(priv->args);
    free(priv);

    free(shared.cpu_names);
    free(shared.cpu_usage);

    memset(&shared, 0, sizeof(shared));

    pthread_cleanup_pop(1);
}

static PrinterPrivateState *
printer_init(void *arg)
{
    PrinterPrivateState *priv;

    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    priv = ecalloc(1, sizeof(*priv));

    priv->args = arg;

    int max_cpu_entries = priv->args->max_cpu_entries;

    shared.max_cpu_entries = max_cpu_entries;
    shared.cpu_names = emalloc((size_t)max_cpu_entries * sizeof(shared.cpu_names[0]));
    shared.cpu_usage = emalloc((size_t)max_cpu_entries * sizeof(shared.cpu_usage[0]));

    shared.printer_initialized = true;

    iret = pthread_cond_signal(&cond_on_printer_initialized);
    assert(iret == 0);

    pthread_cleanup_pop(1);

    return priv;
}

static void
printer_loop(PrinterPrivateState *priv)
{
    while (1) {
        printer_print_usage();

        if (priv->args->use_watchdog) {
            watchdog_signal_active("Printer");
        }
    }
}

void *
printer_run(void *arg)
{
    assert(arg);

    PrinterPrivateState *priv = printer_init(arg);

    pthread_cleanup_push(printer_deinit, priv);

    printer_loop(priv);

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

void
printer_submit_data(int n_cpu_entries, char cpu_names[n_cpu_entries][PROCSTATCPUENTRY_CPU_NAME_SIZE], double cpu_usage[n_cpu_entries])
{
    int iret = pthread_mutex_lock(&printer_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &printer_lock);

    ensure_initialized(&shared.printer_initialized, &cond_on_printer_initialized, &printer_lock);

    if (n_cpu_entries > shared.max_cpu_entries) {
        ELOG(false, "Exceeded max_cpu_entries");
    } else {
        memcpy(shared.cpu_names, cpu_names, (size_t)n_cpu_entries * sizeof(cpu_names[0]));
        memcpy(shared.cpu_usage, cpu_usage, (size_t)n_cpu_entries * sizeof(cpu_usage[0]));
        shared.n_cpu_entries = n_cpu_entries;
        shared.new_data_submitted = true;

        pthread_cond_signal(&cond_on_data_submitted);
    }

    pthread_cleanup_pop(1);
}
