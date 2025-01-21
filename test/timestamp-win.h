#ifndef _TIMESTAMP_WIN_H__
#define _TIMESTAMP_WIN_H__
#ifndef _WIN32
/*
 * This file implementation clock_gettime(CLOCK_MONOTONIC) and clock_nanosleep(CLOCK_MONOTONIC).
 * In mingw-w64 this functions implemented for CLOCK_REALTIME only.
*/
#error Not supported OS!
#endif

#include <fifo-common.h>
uint64_t fifo_get_timestamp(void);
void fifo_wait(const uint64_t abstime);

#endif  /* _TIMESTAMP_WIN_H__ */