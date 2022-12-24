#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysinfo.h> /* get_nprocs_conf() */

#include "utils.h"
#include "proc_stat_utils.h"
#include "reader.h"
#include "analyzer.h"

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

    pthread_t reader;
    pthread_t analyzer;

    ReaderArgs *reader_args = ecalloc(1, sizeof(*reader_args));
    reader_args->max_cpu_entries = max_cpu_entries;

    AnalyzerArgs *analyzer_args = ecalloc(1, sizeof(*analyzer_args));
    analyzer_args->max_cpu_entries = max_cpu_entries;

    int iret;

    iret = pthread_create(&reader, NULL, reader_run, reader_args);
    assert(iret == 0);

    iret = pthread_create(&analyzer, NULL, analyzer_run, analyzer_args);
    assert(iret == 0);


    sleep(15);

    iret = pthread_cancel(reader);
    assert(iret == 0);
    iret = pthread_cancel(analyzer);
    assert(iret == 0);


    iret = pthread_join(reader, NULL);
    assert(iret == 0);

    iret = pthread_join(analyzer, NULL);
    assert(iret == 0);


    pthread_exit(NULL);
}
