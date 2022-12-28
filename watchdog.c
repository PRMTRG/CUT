#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <signal.h>

#include "watchdog.h"
#include "logger.h"
#include "utils.h"
#include "thread_utils.h"

typedef struct {
    char name[64];
    pthread_t id;
    struct timespec last_activity;
} WatchedThread;

static struct {
    bool watchdog_initialized;
    WatchedThread watched_threads[10];
    int n_watched_threads;
} shared;

static pthread_mutex_t watchdog_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_on_watchdog_initialized = PTHREAD_COND_INITIALIZER;

static volatile sig_atomic_t signal_received;

static void
watchdog_handle_signal(int signum)
{
    signal_received = signum;
}

static void
watchdog_check_threads_activity(void)
{
    int iret = pthread_mutex_lock(&watchdog_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &watchdog_lock);

    struct timespec now;
    iret = clock_gettime(CLOCK_MONOTONIC, &now);
    assert(iret == 0);

    for (int i = 0; i < shared.n_watched_threads; i++) {
        struct timespec *last_activity = &shared.watched_threads[i].last_activity;

        double diff = 0;
        diff += now.tv_sec - last_activity->tv_sec;
        diff += (now.tv_nsec - last_activity->tv_nsec) / (1000 * 1000 * 1000);

        if (diff > 2) {
            EPRINT("Thread \"%s\" timed out. Exiting the program.", shared.watched_threads[i].name);
            ELOG(true, "Thread \"%s\" timed out. Exiting the program.", shared.watched_threads[i].name);

            for (int i = 0; i < shared.n_watched_threads; i++) {
                pthread_cancel(shared.watched_threads[i].id);
            }

            pthread_exit(NULL);
        }
    }

    pthread_cleanup_pop(1);
}

static void
watchdog_deinit(void *arg)
{
    (void)(arg);

    int iret = pthread_mutex_lock(&watchdog_lock);
    assert(iret == 0);

    memset(&shared, 0, sizeof(shared));

    signal_received = 0;

    iret = pthread_mutex_unlock(&watchdog_lock);
    assert(iret == 0);
}

static void
watchdog_init(void)
{
    int iret = pthread_mutex_lock(&watchdog_lock);
    assert(iret == 0);

    shared.watchdog_initialized = true;

    sigset_t masked_signals;
    sigemptyset(&masked_signals);
    sigaddset(&masked_signals, SIGTERM);
    iret = pthread_sigmask(SIG_UNBLOCK, &masked_signals, NULL);
    assert(iret == 0);

    struct sigaction sa = {0};
    sa.sa_handler = watchdog_handle_signal;

    iret = sigaction(SIGTERM, &sa, NULL);
    assert(iret == 0);

    iret = pthread_cond_signal(&cond_on_watchdog_initialized);
    assert(iret == 0);

    iret = pthread_mutex_unlock(&watchdog_lock);
    assert(iret == 0);
}

static void
watchdog_loop(void)
{
    while (1) {
        watchdog_check_threads_activity();

        if (signal_received) {
            EPRINT("Received signal %d. Exiting program.", signal_received);
            ELOG(true, "Received signal %d. Exiting program.", signal_received);

            for (int i = 0; i < shared.n_watched_threads; i++) {
                pthread_cancel(shared.watched_threads[i].id);
            }

            pthread_exit(NULL);
        }

        sleep(1);
    }
}

void *
watchdog_run(void *arg)
{
    (void)(arg);

    watchdog_init();

    pthread_cleanup_push(watchdog_deinit, NULL);

    watchdog_loop();

    pthread_cleanup_pop(1);

    pthread_exit(NULL);
}

void
watchdog_signal_active(const char *name)
{
    assert(name);

    int iret = pthread_mutex_lock(&watchdog_lock);
    assert(iret == 0);
    pthread_cleanup_push(cleanup_mutex_unlock, &watchdog_lock);

    ensure_initialized(&shared.watchdog_initialized, &cond_on_watchdog_initialized, &watchdog_lock);

    pthread_t self = pthread_self();

    int i;
    for (i = 0; i < shared.n_watched_threads; i++) {
        if (pthread_equal(shared.watched_threads[i].id, self)) {
            break;
        }
    }
    if (i == shared.n_watched_threads) {
        if ((size_t)shared.n_watched_threads >= sizeof(shared.watched_threads) / sizeof(shared.watched_threads[0])) {
            EPRINT("Exceeded maximum number of watched threads");
            // TODO: Send exit signal
            pthread_exit(NULL);
        }
        shared.n_watched_threads++;
        shared.watched_threads[i].id = self;

        size_t len = strlen(name);
        size_t maxlen = sizeof(shared.watched_threads[0].name);
        if (len > maxlen) {
            len = maxlen;
        }
        char *dest = shared.watched_threads[i].name;
        memcpy(dest, name, len);
        dest[len] = '\0';
    }

    struct timespec ts;
    iret = clock_gettime(CLOCK_MONOTONIC, &ts);
    assert(iret == 0);

    shared.watched_threads[i].last_activity = ts;

    pthread_cleanup_pop(1);
}
