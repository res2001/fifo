#ifndef _FIFO_COMMON_H__
#define _FIFO_COMMON_H__
#include <queue-delays.h>
#include <queue-error.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>

#define UNUSED(x)                           ((void)(x))
#define MAKE_STR_END(x)                     # x
#define MAKE_STR2(x)                        MAKE_STR_END(x)
#define MAKE_STR(x)                         MAKE_STR2(x)
#define NSEC_PER_SEC                        (1000000000)
#define NSEC_PER_MLSEC                      (1000000)

#define FIFO_MALLOC(size)                   malloc(size)
#define FIFO_FREE(ptr)                      free(ptr)
#define FIFO_PRINT                          printf
#define FIFO_PRINT_ERROR(...)               fprintf(stderr, __VA_ARGS__)
#define FIFO_STRERROR(err)                  strerror(err)
#define FIFO_CLOCK_GETTIME(timespec)        clock_gettime(CLOCK_REALTIME, timespec)

#define FIFO_TIMESPEC_ADD_TIMEOUT(ts, timeout)                      \
    do {                                                            \
        (ts).tv_sec  += (long long)((timeout) / NSEC_PER_SEC);      \
        (ts).tv_nsec += (long)((timeout) % NSEC_PER_SEC);           \
        if((ts).tv_nsec >= NSEC_PER_SEC)                            \
        {                                                           \
            (ts).tv_nsec -= NSEC_PER_SEC;                           \
            ++(ts).tv_sec;                                          \
        }                                                           \
    } while(0)

/* mutex */
#define FIFO_MTX_TYPE                       pthread_mutex_t
#define FIFO_MTX_DECLARE(mtx_name)          FIFO_MTX_TYPE mtx_name
#define FIFO_MTX_INIT(mtx_ptr)              pthread_mutex_init(mtx_ptr, NULL)
#define FIFO_MTX_LOCK(mtx_ptr)              pthread_mutex_lock(mtx_ptr)
#define FIFO_MTX_TRYLOCK(mtx_ptr)           pthread_mutex_trylock(mtx_ptr)
#define FIFO_MTX_UNLOCK(mtx_ptr)            pthread_mutex_unlock(mtx_ptr)
#define FIFO_MTX_DESTROY(mtx_ptr)           pthread_mutex_destroy(mtx_ptr)

/* condition variable */
#define FIFO_CND_TYPE                       pthread_cond_t
#define FIFO_CND_DECLARE(cnd_name)          FIFO_CND_TYPE cnd_name
#define FIFO_CND_INIT(cnd_ptr)              pthread_cond_init(cnd_ptr, NULL)
#define FIFO_CND_SIGNAL(cnd_ptr)            pthread_cond_signal(cnd_ptr)
#define FIFO_CND_BROADCAST(cnd_ptr)         pthread_cond_broadcast(cnd_ptr)
#define FIFO_CND_WAIT(cnd_ptr, mtx_ptr)     pthread_cond_wait((cnd_ptr), (mtx_ptr))
#define FIFO_CND_TIMEDWAIT(cnd_ptr, mtx_ptr, timespec_ptr)       \
                                            pthread_cond_timedwait((cnd_ptr), (mtx_ptr), (timespec_ptr))
#define FIFO_CND_DESTROY(cnd_ptr)           pthread_cond_destroy(cnd_ptr)

#define FIFO_THREAD_TYPE                    pthread_t
#define FIFO_THREAD_DECLARE(thread_name)    PBM_THREAD_TYPE thread_name
#define FIFO_THREAD_CREATE(th_ptr, start_routine, arg_ptr, tname)        \
    pthread_create((th_ptr), NULL, (start_routine), (arg_ptr))
#define FIFO_THREAD_JOIN(th, void_pptr)     pthread_join((th), (void**)(void_pptr))
#define FIFO_THREAD_YIELD()                 sched_yield()

#endif  /* _FIFO_COMMON_H__ */