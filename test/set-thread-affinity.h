#ifndef _SETTHREADAFFINITY_H__
#define _SETTHREADAFFINITY_H__

#if defined(_WIN32) || defined(__linux__)

int set_thread_affinity(void);

#else

#define set_thread_affinity()       0

#endif

#endif /* _SETTHREADAFFINITY_H__ */
