#ifndef __QUEUE_ERROR_H__
#define __QUEUE_ERROR_H__
#include <errno.h>

enum queue_error_e {
    QUEUE_SUCCESS           = 0,
    QUEUE_ERR_NOT_MEMORY    = ENOMEM,
    QUEUE_ERR_TIMEDOUT      = ETIMEDOUT,
    QUEUE_ERR_BUSY          = EBUSY,
    QUEUE_AGAIN             = 1000,
    QUEUE_EMPTY             = 1001,
    QUEUE_STOP              = 1002,
};

#endif  // __QUEUE_ERROR_H__
