#define _GNU_SOURCE
#include <fifo-common.h>
#include "set-thread-affinity.h"

#if defined(_WIN32)

#include <windows.h>
#include <last-error-win.h>
#include <bit-scan-revers.h>

int set_thread_affinity(void)
{
    DWORD_PTR proc_affinity = 0;
    DWORD_PTR sys_affinity = 0;
    if(!GetProcessAffinityMask(GetCurrentProcess(), & proc_affinity, & sys_affinity))
    {
        last_error_msg("GetProcessAffinityMask failed. ", GetLastError());
        return -1;
    }

    if(proc_affinity == 0)
    {
        PBM_CORE_WARNING("Process affinity mask is zero!");
        proc_affinity = 1;
    } else {
        proc_affinity = msb64(proc_affinity);
    }

    if(SetThreadAffinityMask(GetCurrentThread(), proc_affinity) != 0)
    {
        sched_yield();
        return 0;
    }

    last_error_msg("SetThreadAffinityMask failed. ", GetLastError());
    return -1;
}

#elif defined(__linux__)
#include <sched.h>

int set_thread_affinity(void)
{
    cpu_set_t cpuset;
    CPU_ZERO(& cpuset);
    CPU_SET(1, & cpuset);

    const int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), & cpuset);
    if(ret == 0)
    {
        sched_yield();
        return 0;
    }

    FIFO_PRINT_ERROR("pthread_setaffinity_np error: %s (%d)", FIFO_STRERROR(ret), ret);
    return -1;
}

#endif  // _WIN32

