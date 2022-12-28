#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "logger.h"
#include "utils.h"
#include "thread_utils.h"
#include "watchdog.h"

typedef struct {
    char message[512];
    char *message_remainder;
} LoggerQueueEntry;

typedef struct {
    LoggerArgs *args;
    FILE *log_file;
} LoggerPrivateState;

static struct {
    bool logger_initialized;
    LoggerQueueEntry *logger_queue;
    int n_queue_slots;
    int n_queued;
} shared;

static pthread_mutex_t logger_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_logger_initialized = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_queue_emptied = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_on_message_submitted = PTHREAD_COND_INITIALIZER;

/*
 * Logger lock must be acquired before calling this function.
 */
static void
logger_write_queued_messages_to_log_file(FILE *log_file)
{
    for (int i = 0; i < shared.n_queued; i++) {
        LoggerQueueEntry *queue_entry = &shared.logger_queue[i];
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
    shared.n_queued = 0;
}

static void
logger_handle_queued_messages(FILE *log_file)
{
    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &logger_lock);

    if (shared.n_queued == 0) {
        cond_wait_seconds(&cond_on_message_submitted, &logger_lock, 1);
    }

    if (shared.n_queued > 0) {
        iret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        assert(iret == 0);

        logger_write_queued_messages_to_log_file(log_file);

        iret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        assert(iret == 0);

        iret = pthread_cond_broadcast(&cond_on_queue_emptied);
        assert(iret == 0);
    }

    pthread_cleanup_pop(1);
}

static void
logger_deinit(void *arg)
{
    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &logger_lock);

    LoggerPrivateState *priv = arg;

    logger_write_queued_messages_to_log_file(priv->log_file);

    if (priv->log_file) {
        iret = fclose(priv->log_file);
        assert(iret == 0);
    }

    free(priv->args);
    free(priv);

    free(shared.logger_queue);

    memset(&shared, 0, sizeof(shared));

    pthread_cleanup_pop(1);
}

static LoggerPrivateState *
logger_init(void *arg)
{
    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);

    LoggerPrivateState *priv = ecalloc(1, sizeof(*priv));

    priv->args = arg;

    const char *log_file_name = "log.txt";

    priv->log_file = fopen(log_file_name, "a");
    if (!priv->log_file) {
        EPRINT("Failed to open log file (%s)", log_file_name);

        iret = pthread_mutex_unlock(&logger_lock);
        assert(iret == 0);

        logger_deinit(priv);
        pthread_exit(NULL);
    }

    int n_queue_slots = 5;

    shared.n_queue_slots = n_queue_slots;

    shared.logger_queue = emalloc((size_t)n_queue_slots * sizeof(shared.logger_queue[0]));

    shared.logger_initialized = true;

    iret = pthread_cond_broadcast(&cond_on_logger_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&logger_lock);
    assert(iret == 0);

    return priv;
}

static void
logger_loop(LoggerPrivateState *priv)
{
    while (1) {
        logger_handle_queued_messages(priv->log_file);

        if (priv->args->use_watchdog) {
            watchdog_signal_active("Logger");
        }
    }
}

void *
logger_run(void *arg)
{
    assert(arg);

    LoggerPrivateState *priv = logger_init(arg);

    pthread_cleanup_push(logger_deinit, priv);

    logger_loop(priv);

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

void
logger_log_message(bool dont_block_on_fail, const char *message)
{
    assert(message);

    int iret = pthread_mutex_lock(&logger_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &logger_lock);

    bool can_log = true;

    if (!dont_block_on_fail) {
        ensure_initialized(&shared.logger_initialized, &cond_on_logger_initialized, &logger_lock);
    }

    if (!shared.logger_initialized) {
        can_log = false;
    }

    while (shared.n_queued >= shared.n_queue_slots) {
        if (dont_block_on_fail) {
            can_log = false;
            break;
        }
        iret = pthread_cond_wait(&cond_on_queue_emptied, &logger_lock);
        assert(iret != EINVAL);
        assert(iret != EPERM);
    }

    if (can_log) {
        LoggerQueueEntry *queue_slot = &shared.logger_queue[shared.n_queued];

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
                EPRINT("malloc() failed. (non-critical)");
            } else {
                memcpy(rem_str, &message[main_length], remainder_length + 1);
            }
            queue_slot->message_remainder = rem_str;
        }

        shared.n_queued++;

        iret = pthread_cond_signal(&cond_on_message_submitted);
        assert(iret == 0);
    }

    pthread_cleanup_pop(1);
}
