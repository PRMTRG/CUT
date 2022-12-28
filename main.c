#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/sysinfo.h> /* get_nprocs_conf() */

#include "utils.h"
#include "proc_stat_utils.h"
#include "reader.h"
#include "analyzer.h"
#include "printer.h"
#include "logger.h"
#include "watchdog.h"

int
main(int argc, char **argv)
{
    (void)(argc); (void)(argv);

    /*
     * get_nprocs_conf() returns the number of CPUs configured by the operating system.
     * Data structures used for processing the /proc/stat file will be initialized with a number of
     * slots equal to this value + 1 (we need a slot for each of the cores + one more for the average CPU usage).
     * The returned value can be different than the actual number of active processors in the system.
     * If during the run time of the program the number of CPU per-code entries read out from
     * the /proc/stat file exceeds this value the program will exit.
     */
    int nprocs = get_nprocs_conf();
    int max_cpu_entries = nprocs + 1;

    int iret;

    /*
     * Block signals so that Watchdog can later unblock and handle them.
     */
    sigset_t masked_signals;
    sigemptyset(&masked_signals);
    sigaddset(&masked_signals, SIGTERM);
    iret = pthread_sigmask(SIG_BLOCK, &masked_signals, NULL);
    assert(iret == 0);

    pthread_t watchdog;
    pthread_t reader;
    pthread_t analyzer;
    pthread_t printer;
    pthread_t logger;

    ReaderArgs *reader_args = ecalloc(1, sizeof(*reader_args));
    reader_args->max_cpu_entries = max_cpu_entries;
    reader_args->use_watchdog = true;

    AnalyzerArgs *analyzer_args = ecalloc(1, sizeof(*analyzer_args));
    analyzer_args->max_cpu_entries = max_cpu_entries;
    analyzer_args->use_watchdog = true;

    PrinterArgs *printer_args = ecalloc(1, sizeof(*printer_args));
    printer_args->max_cpu_entries = max_cpu_entries;
    printer_args->use_watchdog = true;

    LoggerArgs *logger_args = ecalloc(1, sizeof(*logger_args));
    logger_args->use_watchdog = true;

    iret = pthread_create(&watchdog, NULL, watchdog_run, NULL);
    assert(iret == 0);

    iret = pthread_create(&reader, NULL, reader_run, reader_args);
    assert(iret == 0);

    iret = pthread_create(&analyzer, NULL, analyzer_run, analyzer_args);
    assert(iret == 0);

    iret = pthread_create(&printer, NULL, printer_run, printer_args);
    assert(iret == 0);

    iret = pthread_create(&logger, NULL, logger_run, logger_args);
    assert(iret == 0);

    iret = pthread_join(reader, NULL);
    assert(iret == 0);

    iret = pthread_join(analyzer, NULL);
    assert(iret == 0);

    iret = pthread_join(printer, NULL);
    assert(iret == 0);

    iret = pthread_join(logger, NULL);
    assert(iret == 0);

    iret = pthread_join(watchdog, NULL);
    assert(iret == 0);

    pthread_exit(NULL);
}
