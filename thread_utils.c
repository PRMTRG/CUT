#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#include "thread_utils.h"

void
cleanup_mutex_unlock(void *mutex)
{
    int iret = pthread_mutex_unlock(mutex);
    assert(iret == 0);
}

void
cleanup_fclose(void *file)
{
    int iret = fclose(file);
    assert(iret == 0);
}
