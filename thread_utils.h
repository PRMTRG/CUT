#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>

void cleanup_mutex_unlock(void *mutex);

void cond_wait_seconds(pthread_cond_t cond[static 1], pthread_mutex_t mutex[static 1], int seconds);

static inline void
ensure_initialized(bool is_initialized[static 1], pthread_cond_t cond_on_initialized[static 1], pthread_mutex_t mutex[static 1])
{
    while (!*is_initialized) {
        int iret = pthread_cond_wait(cond_on_initialized, mutex);
        assert(iret != EINVAL);
        assert(iret != EPERM);
    }
}

#endif /* THREAD_UTILS_H */
