#ifndef _WIN32
/*
 * This file implementation clock_gettime(CLOCK_MONOTONIC) and clock_nanosleep(CLOCK_MONOTONIC).
 * In mingw-w64 this functions implemented for CLOCK_REALTIME only.
*/
#error Not supported OS!
#endif

#include <fifo-common.h>
#include <profileapi.h>
#include <synchapi.h>
#include <inttypes.h>

static LARGE_INTEGER pf = { .QuadPart = 0 };

uint64_t fifo_get_timestamp(void)
{
    if(pf.QuadPart == 0)
    {
        QueryPerformanceFrequency(& pf);
//        PBM_PRINT("QueryPerformanceFrequency: %" PRIuPTR "\n", (uint64_t)pf.QuadPart);
    }

    LARGE_INTEGER pc = { .QuadPart = 0 };
    QueryPerformanceCounter(& pc);

    const uint64_t sec = (uint64_t)(pc.QuadPart / pf.QuadPart);
    const uint64_t ns = (uint64_t)(pc.QuadPart % pf.QuadPart * NSEC_PER_SEC / pf.QuadPart);
    return sec * NSEC_PER_SEC + ns;
//    return (uint64_t)((double)pc.QuadPart / (double)pf.QuadPart * NSEC_PER_SEC);
}

void fifo_wait(const uint64_t abstime)
{
    uint64_t curtime = fifo_get_timestamp();
    if(curtime >= abstime) return;
    DWORD diff_ms = (DWORD)((abstime - curtime) / NSEC_PER_MLSEC);
    if(diff_ms > 0)
    {
        Sleep(diff_ms);
        curtime = fifo_get_timestamp();
    }
    while(curtime < abstime)
    {
        curtime = fifo_get_timestamp();
    }
}
