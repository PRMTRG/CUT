#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "logger.h"
#include "utils.h"
#include "thread_utils.h"

typedef struct {
    char message[512];
    char *message_remainder;
} LoggerQueueEntry;

static pthread_mutex_t logger_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_logger_initialized = PTHREAD_COND_INITIALIZER;
static bool logger_initialized;

static const char *log_file_name = "log.txt";
static FILE *log_file;

static pthread_cond_t cond_on_queue_emptied = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_message_submitted = PTHREAD_COND_INITIALIZER;
static LoggerQueueEntry *logger_queue;
static int n_queue_slots;
static int n_queued;

/*
 * Logger lock must be acquired before calling this function.
 */
static void
write_queued_messages_to_log_file(void)
{
    for (int i = 0; i < n_queued; i++) {
        LoggerQueueEntry *queue_entry = &logger_queue[i];
        int iret = fputs(queue_entry->message, log_file);
        assert(iret != EOF);
        if (queue_entry->message_remainder) {
            iret = fputs(queue_entry->message_remainder, log_file);
            assert(iret != EOF);
            free(queue_entry->message_remainder);
        }
        iret = fputc('\n', log_file);
        assert(iret != EOF);
    }
    n_queued = 0;
}

static void
handle_queued_messages(void)
{
    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &logger_lock);

    if (n_queued == 0) {
        // TODO: Make this a timed wait
        iret = pthread_cond_wait(&cond_on_message_submitted, &logger_lock);
        assert(iret != EINVAL);
        assert(iret != EPERM);
    }

    if (n_queued > 0) {
        write_queued_messages_to_log_file();

        iret = pthread_cond_broadcast(&cond_on_queue_emptied);
        assert(iret == 0);
    }

    pthread_cleanup_pop(1);
}

static void
deinit(void *arg)
{
    (void)(arg);

    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);

    write_queued_messages_to_log_file();

    logger_initialized = false;
    n_queue_slots = 0;
    n_queued = 0;

    iret = pthread_mutex_unlock(&logger_lock);
    assert(iret == 0);
}

void *
logger_run(void *arg)
{
    pthread_cleanup_push(free, arg);

    log_file = fopen(log_file_name, "a");
    if (!log_file) {
        eprint("Failed to open log file (%s)", log_file_name);
        exit(EXIT_FAILURE);
    }
    pthread_cleanup_push(cleanup_fclose, log_file);

    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);

    n_queue_slots = 5;

    logger_queue = emalloc((size_t)n_queue_slots * sizeof(logger_queue[0]));
    pthread_cleanup_push(free, logger_queue);

    logger_initialized = true;
    iret = pthread_cond_broadcast(&cond_on_logger_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&logger_lock);
    assert(iret == 0);

    pthread_cleanup_push(deinit, NULL);

    while (1) {
        handle_queued_messages();

        // TODO: signal to watchdog
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

void
logger_log_message(const char *message)
{
    assert(message);

    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &logger_lock);

    ensure_initialized(&logger_initialized, &cond_on_logger_initialized, &logger_lock);

    while (n_queued >= n_queue_slots) {
        iret = pthread_cond_wait(&cond_on_queue_emptied, &logger_lock);
        assert(iret != EINVAL);
        assert(iret != EPERM);
    }

    LoggerQueueEntry *queue_slot = &logger_queue[n_queued];

    size_t total_length = strlen(message);
    size_t main_length;
    size_t remainder_length;

    const size_t max_main_length = sizeof(queue_slot->message) - 1;

    if (total_length > max_main_length) {
        main_length = max_main_length;
        remainder_length = total_length - max_main_length;
    } else {
        main_length = total_length;
        remainder_length = 0;
    }

    memcpy(queue_slot->message, message, main_length);
    queue_slot->message[main_length] = '\0';

    if (remainder_length == 0) {
        queue_slot->message_remainder = NULL;
    } else {
        /* Heap-allocated messages are freed in write_queued_messages_to_log_file() */
        char *rem_str = malloc(remainder_length + 1);
        if (!rem_str) {
            eprint("malloc() failed. (non-critical)");
        } else {
            memcpy(rem_str, &message[main_length], remainder_length + 1);
        }
        queue_slot->message_remainder = rem_str;
    }

    n_queued++;

    iret = pthread_cond_signal(&cond_on_message_submitted);
    assert(iret == 0);

    pthread_cleanup_pop(1);
}
