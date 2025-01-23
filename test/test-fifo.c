#include <fifo-common.h>
#include "test-fifo.h"

#include <getopt.h>
#include <stdlib.h>
#include <time.h>

fifo_params_t params;
enum log_level_e log_level = LOG_NOTICE;
const char *log_level_name[] = {"ERR", "WRN", "NTC", "INF", "DBG"};

uint64_t get_hrtime(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, & tv);
    return (uint64_t)(tv.tv_nsec + tv.tv_sec * NSEC_PER_SEC);
}

static void usage(const char *prog_name)
{
    FIFO_PRINT_ERROR("Usage: %s [options]\n\n"
            "  -h, --help       display help information\n\n"
            "  -l, --log_level  Log level (one of 0 - DEBUG, 1 - INFO, 2 - NOTICE, 3 - WARNING or 4 - ERROR).\n"
            "                   Default value: 2.\n\n"
            "  -a, --addthread  The number of threads adds. Default value: " MAKE_STR(TEST_THREAD_DEFAULT) ".\n\n"
            "  -d, --delthread  The number of threads remover. Default value: " MAKE_STR(TEST_THREAD_DEFAULT) ".\n\n"
            "  -i, --num_node_per_thread\n"
            "                   The number of nodes in each thread adds. Default value: " MAKE_STR(POOL_NODE_SIZE) ".\n"
            "  -p, --pool_size\n"
            "                   Pool size for tests that use the pool. Default value: " MAKE_STR(POOL_SIZE) ".\n"
            "  -b, --benchmark\n"
            "                   this is benchmark.\n"
                    , prog_name);
}

// Разбор параметров командной строки
static int parse_args(int argc, char *argv[])
{
    params.addthread = TEST_THREAD_DEFAULT;
    params.delthread = TEST_THREAD_DEFAULT;
    params.num_node_per_thread = POOL_NODE_SIZE;
    params.pool_size = POOL_SIZE;
    params.is_bench = false;
    int level = LOG_NOTICE;

    const struct option long_options[] = {
                {"help",        no_argument,        0, 'h' },
                {"log_level",   required_argument,  0, 'l' },
                {"addthread",   required_argument,  0, 'a' },
                {"delthread",   required_argument,  0, 'd' },
                {"num_node_per_thread",     required_argument,  0, 'i' },
                {"pool_size",   required_argument,  0, 'p' },
                {"bench",       no_argument,        0, 'b' },
                {0,         0,                 0,  0 }
    };

    int c;
    while( (c = getopt_long(argc, argv, "a:d:p:i:l:bh", long_options, NULL)) != -1)
    {
        switch(c)
        {
        case 'a':
            params.addthread = (int)strtol(optarg, NULL, 0);
            break;
        case 'b':
            params.is_bench = true;
            break;
        case 'd':
            params.delthread = (int)strtol(optarg, NULL, 0);
            break;
        case 'i':
            params.num_node_per_thread = (int)strtol(optarg, NULL, 0);
            break;
        case 'p':
            params.pool_size = (int)strtol(optarg, NULL, 0);
            break;
        case 'l':
            level = (int)strtol(optarg, NULL, 0);
            break;
        case 'h':
            usage(argv[0]);
            return -1;
        case '?':
        default:
            usage(argv[0]);
            return -2;
        }
    }

    int ret = 0;
    if(params.addthread < TEST_THREAD_MIN || params.addthread > TEST_THREAD_MAX)
    {
        LOG_WRITE_ERROR("addthread option must be in the range from %d to %d.\n", TEST_THREAD_MIN, TEST_THREAD_MAX);
        ++ret;
    }
    if(params.delthread < TEST_THREAD_MIN || params.delthread > TEST_THREAD_MAX)
    {
        LOG_WRITE_ERROR("delthread option must be in the range from %d to %d.\n", TEST_THREAD_MIN, TEST_THREAD_MAX);
        ++ret;
    }
    if(params.num_node_per_thread < POOL_NODE_SIZE_MIN || params.num_node_per_thread > POOL_NODE_SIZE_MAX)
    {
        LOG_WRITE_ERROR("num_node_per_thread option must be in the range from %d to %d.\n", POOL_NODE_SIZE_MIN, POOL_NODE_SIZE_MAX);
        ++ret;
    }
    if(params.pool_size < POOL_SIZE_MIN || params.pool_size > POOL_SIZE_MAX)
    {
        LOG_WRITE_ERROR("pool_size option must be in the range from %d to %d.\n", POOL_SIZE_MIN, POOL_SIZE_MAX);
        ++ret;
    }
    if(level > LOG_DEBUG || level < LOG_ERROR)
    {
        LOG_WRITE_ERROR("log_level option must be in the range from %d to %d.\n", LOG_DEBUG, LOG_ERROR);
        ++ret;
    } else {
        log_level = level;
    }
    return ret;
}

int main(int argc, char *argv[])
{
    if(parse_args(argc, argv))
    {
        return 1;
    }
    srand ((unsigned int)time(NULL));

    int number_failed;
    SRunner *sr = srunner_create(fix_allocator_suite());
    srunner_add_suite(sr, fifo_tlq_fix_suite());
    srunner_add_suite(sr, fifo_fn_fix_suite());
    srunner_add_suite(sr, fifo_lfi_suite());
    srunner_add_suite(sr, fifo_tlqi_suite());
    srunner_add_suite(sr, fifo_li_suite());

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
