#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "thread_utils.h"

void
cleanup_mutex_unlock(void *mutex)
{
    int iret = pthread_mutex_unlock(mutex);
    assert(iret == 0);
}

void
cond_wait_seconds(pthread_cond_t cond[static 1], pthread_mutex_t mutex[static 1], int seconds)
{
    struct timespec ts;
    int iret = clock_gettime(CLOCK_REALTIME, &ts);
    assert(iret == 0);

    ts.tv_sec += seconds;

    iret = pthread_cond_timedwait(cond, mutex, &ts);
    assert(iret != EINVAL);
    assert(iret != EPERM);
}
