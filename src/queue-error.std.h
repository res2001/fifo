#ifndef __QUEUE_ERROR_H__
#define __QUEUE_ERROR_H__
#include <threads.h>

enum queue_error_e {
    QUEUE_SUCCESS = thrd_success,
    QUEUE_ERR_NOT_MEMORY = thrd_nomem,
    QUEUE_ERR_TIMEDOUT = thrd_timedout,
    QUEUE_ERR_BUSY = thrd_busy,
    QUEUE_ERR = thrd_error,
    QUEUE_EMPTY = 5,
    QUEUE_STOP = 6,
};

#endif  // __QUEUE_ERROR_H__
