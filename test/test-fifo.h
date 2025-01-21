#ifndef TEST_FIFO_H_
#define TEST_FIFO_H_
#include <fifo-common.h>
#include <check.h>
#include <inttypes.h>

#define MILLISEC				1000
#define NANOSEC					1000000000

#define TEST_THREAD_MIN			1
#define TEST_THREAD_MAX			100
#define TEST_THREAD_DEFAULT		2
#define POOL_NODE_SIZE          10000
#define POOL_NODE_SIZE_MIN		1000
#define POOL_NODE_SIZE_MAX		1000000000
#define POOL_SIZE               0
#define POOL_SIZE_MIN           0
#define POOL_SIZE_MAX           1000000000

typedef struct
{
    int addthread;
    int delthread;
    int num_node_per_thread;
    int pool_size;
} fifo_params_t;

extern fifo_params_t params;
uint64_t get_hrtime(void);

enum log_level_e {
    LOG_ERROR,
    LOG_WARNING,
    LOG_NOTICE,
    LOG_INFO,
    LOG_DEBUG,
};
extern enum log_level_e log_level;
extern const char *log_level_name[];

#define LOG_WRITE(level, fmt, ...)              \
    do {                                        \
        if(level <= log_level) {                \
            fprintf(stderr, "%s %s(%d): " fmt, log_level_name[level], __func__, __LINE__, ## __VA_ARGS__);    \
    } } while(0)

#define LOG_WRITE_ERROR(fmt, ...)               LOG_WRITE(LOG_ERROR, fmt, ## __VA_ARGS__)
#define LOG_WRITE_WARNING(fmt, ...)             LOG_WRITE(LOG_WARNING, fmt, ## __VA_ARGS__)
#define LOG_WRITE_NOTICE(fmt, ...)              LOG_WRITE(LOG_NOTICE, fmt, ## __VA_ARGS__)
#define LOG_WRITE_INFO(fmt, ...)                LOG_WRITE(LOG_INFO, fmt, ## __VA_ARGS__)
#define LOG_WRITE_DEBUG(fmt, ...)               LOG_WRITE(LOG_DEBUG, fmt, ## __VA_ARGS__)

Suite* fix_allocator_suite(void);
Suite* fifo_tlq_fix_suite(void);
Suite* fifo_fn_fix_suite(void);
Suite* fifo_tlqi_suite(void);

#endif /* TEST_FIFO_H_ */
