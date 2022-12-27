#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#include "utils.h"
#include "proc_stat_utils.h"
#include "reader.h"
#include "analyzer.h"
#include "printer.h"
#include "logger.h"

static void
test_proc_stat_parse(void)
{
    const char *proc_stat_file_name = "/proc/stat";

    int max_cpu_entries = get_nprocs() + 1;

    int iret;

    pthread_t logger;
    iret = pthread_create(&logger, NULL, logger_run, NULL);
    assert(iret == 0);

    /*
     * Test that the correct number of entries gets parsed from /proc/stat
     * and that the file position gets reset to beginning.
     */
    {
        FILE *proc_stat_file = fopen(proc_stat_file_name, "r");
        assert(proc_stat_file);

        ProcStatCpuEntry cpu_entries[max_cpu_entries];

        int n_cpu_entries = read_and_parse_proc_stat_file(proc_stat_file, max_cpu_entries, cpu_entries);

        /* Correct number of entries got parsed */
        assert(n_cpu_entries == max_cpu_entries);

        /* File position was reset to beginning */
        assert(ftell(proc_stat_file) == 0);

        assert(fclose(proc_stat_file) == 0);
    }

    /*
     * Test that the parsing function returns -1 if the provided cpu_entries array is too small.
     * Note: This test won't run if your machine has only 3 or less cores/threads.
     */
    if (max_cpu_entries > 4) {
        FILE *proc_stat_file = fopen(proc_stat_file_name, "r");
        assert(proc_stat_file);

        ProcStatCpuEntry cpu_entries[4];

        int ret = read_and_parse_proc_stat_file(proc_stat_file, 4, cpu_entries);

        /* Parsing fails if the cpu_entries array is too small */
        assert(ret < 0);

        assert(fclose(proc_stat_file) == 0);
    }

    iret = pthread_cancel(logger);
    assert(iret == 0);
    iret = pthread_join(logger, NULL);
    assert(iret == 0);

    printf("%s OK\n", __func__);
}

static char short_message[] = "short message";
static char long_message[] = "very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string very long string";

static void *
logger_message_thread_run(void *arg)
{
    char *msg = arg;

    logger_log_message(arg);

    pthread_exit(NULL);
}

static void
test_logger_long_message(void)
{
    /*
     * Start the logger and two other threads - one will submit a short message to
     * the logger, and the other a long one.
     * Verify that both messages have been correctly written to the log file.
     */

    rename("log.txt", "log.txt.bak");

    pthread_t logger;
    pthread_t thread_a;
    pthread_t thread_b;

    int iret;

    iret = pthread_create(&logger, NULL, logger_run, NULL);
    assert(iret == 0);
    iret = pthread_create(&thread_a, NULL, logger_message_thread_run, short_message);
    assert(iret == 0);
    iret = pthread_create(&thread_b, NULL, logger_message_thread_run, long_message);
    assert(iret == 0);

    iret = pthread_join(thread_a, NULL);
    assert(iret == 0);
    iret = pthread_join(thread_b, NULL);
    assert(iret == 0);

    iret = pthread_cancel(logger);
    assert(iret == 0);
    iret = pthread_join(logger, NULL);
    assert(iret == 0);

    FILE *log_file = fopen("log.txt", "r");
    assert(log_file);

    iret = fseek(log_file, 0, SEEK_END);
    assert(iret == 0);
    int size = ftell(log_file);

    iret = fseek(log_file, 0, SEEK_SET);
    assert(iret == 0);

    char *contents = emalloc((size_t)size);

    iret = fread(contents, 1, size, log_file);
    assert(iret == size);

    iret = fclose(log_file);
    assert(iret == 0);

    /* Search for short message */
    assert(strstr(contents, short_message));

    /* Search for long message */
    assert(strstr(contents, long_message));

    free(contents);

    printf("%s OK\n", __func__);
}

static void *
logger_multiple_messages_thread_run(void *arg)
{
    (void)(arg);

    for (int i = 0; i < 50; i++) {
        logger_log_message(short_message);
        logger_log_message(long_message);
    }

    pthread_exit(NULL);
}

static void
test_logger_many_messages(void)
{
    /*
     * Start the logger and 100 other threads, each one will submit 100 messages to the logger.
     * Verify that all the messages have been written.
     */

    rename("log.txt", "log.txt.bak");

    pthread_t logger;
    pthread_t threads[100];

    int n_threads = sizeof(threads) / sizeof(threads[0]);

    int iret;

    iret = pthread_create(&logger, NULL, logger_run, NULL);
    assert(iret == 0);

    for (int i = 0; i < n_threads; i++) {
        iret = pthread_create(&threads[i], NULL, logger_multiple_messages_thread_run, NULL);
        assert(iret == 0);
    }

    for (int i = 0; i < n_threads; i++) {
        iret = pthread_join(threads[i], NULL);
        assert(iret == 0);
    }

    iret = pthread_cancel(logger);
    assert(iret == 0);
    iret = pthread_join(logger, NULL);
    assert(iret == 0);

    FILE *log_file = fopen("log.txt", "r");
    assert(log_file);

    int lines = 0;
    while (!feof(log_file)) {
        char ch = fgetc(log_file);
        if (ch == '\n') {
            lines++;
        }
    }

    /* Check the number of lines is correct (i.e. all the messages have been written) */
    assert(lines == 100 * 100);

    iret = fclose(log_file);
    assert(iret == 0);

    printf("%s OK\n", __func__);
}

static void
test_restarting_threads(void)
{
    /*
     * Start and stop all threads 3 times to verify that no deadlocks or
     * resource leaks occur.
     */

    int max_cpu_entries = get_nprocs_conf() + 1;

    for (int i = 0; i < 3; i++) {

        pthread_t reader;
        pthread_t analyzer;
        pthread_t printer;
        pthread_t logger;

        ReaderArgs *reader_args = ecalloc(1, sizeof(*reader_args));
        reader_args->max_cpu_entries = max_cpu_entries;

        AnalyzerArgs *analyzer_args = ecalloc(1, sizeof(*analyzer_args));
        analyzer_args->max_cpu_entries = max_cpu_entries;

        PrinterArgs *printer_args = ecalloc(1, sizeof(*printer_args));
        printer_args->max_cpu_entries = max_cpu_entries;

        int iret;

        iret = pthread_create(&reader, NULL, reader_run, reader_args);
        assert(iret == 0);
        iret = pthread_create(&analyzer, NULL, analyzer_run, analyzer_args);
        assert(iret == 0);
        iret = pthread_create(&printer, NULL, printer_run, printer_args);
        assert(iret == 0);
        iret = pthread_create(&logger, NULL, logger_run, NULL);
        assert(iret == 0);

        sleep(5);

        iret = pthread_cancel(reader);
        assert(iret == 0);
        iret = pthread_cancel(analyzer);
        assert(iret == 0);
        iret = pthread_cancel(printer);
        assert(iret == 0);
        iret = pthread_cancel(logger);
        assert(iret == 0);

        iret = pthread_join(reader, NULL);
        assert(iret == 0);
        iret = pthread_join(analyzer, NULL);
        assert(iret == 0);
        iret = pthread_join(printer, NULL);
        assert(iret == 0);
        iret = pthread_join(logger, NULL);
        assert(iret == 0);
    }

    printf("%s OK\n", __func__);
}

int
main(int argc, char **argv)
{
    (void)(argc); (void)(argv);

    test_restarting_threads();
    test_proc_stat_parse();
    test_logger_long_message();
    test_logger_many_messages();

}
