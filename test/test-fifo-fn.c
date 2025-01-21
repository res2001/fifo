#include <fifo-common.h>
#include "set-thread-affinity.h"
#include <fifo-fake-node-fix.h>
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>

#if defined(__linux__)
#   define FIFO_TIMESTAMP_NS        get_hrtime()
#elif defined(_WIN32)
#   include "timestamp-win.h"
#   include "last-error-win.h"
#   define FIFO_TIMESTAMP_NS        fifo_get_timestamp()
#endif

#define TEST_COUNT              1000
#define POOL_SIZE               1000
#define NODE_SIZE               600000

typedef struct
{
    int num;
    int num_node_per_thread;
    int pool_size;
} fifo_params_t;

fifo_params_t params;

typedef void* (*thread_func_t)(void*);

typedef struct
{
    fifo_head_fn_fix_t * head;
    int *data_pool;
    int64_t sum;
    int add;
    uint32_t countok, counterr, countmiss;
} thread_adding_arg_t;

typedef struct
{
    fifo_head_fn_fix_t * head;
    int64_t sum;
    uint32_t countok, countbusy, counterr, countmiss;
} thread_deleting_arg_t;

#if defined(__linux__)
static uint64_t get_hrtime(void)
{
    struct timespec tv;
    clock_gettime(CLOCK_MONOTONIC, & tv);
    return (uint64_t)(tv.tv_nsec + tv.tv_sec * NSEC_PER_SEC);
}
#endif

static void* fn_adding(void * arg)
{
    assert(arg);
    thread_adding_arg_t * s = (thread_adding_arg_t*) arg;
    int32_t i = 0;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(i < params.num_node_per_thread)
    {
        const int32_t data = i + s->add;
        s->data_pool[i] = data;
        const int ret = fifo_fn_fix_push_back(s->head, s->data_pool + i);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            s->sum += data;
            ++s->countok;
            ++i;
            break;
        case QUEUE_ERR_NOT_MEMORY:
            ++s->countmiss;
            FIFO_THREAD_YIELD();
            break;
        case QUEUE_STOP:
            return 0;
        default:
            ++s->counterr;
            break;
        }
    }
    return 0;
}

static void* fn_deleting(void * arg)
{
    assert(arg);
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    int32_t * data;
    int32_t i = 0;
    int32_t prev;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(i < params.num_node_per_thread)
    {
        data = NULL;
        const int ret = fifo_fn_fix_pop_first(s->head, (void**)& data);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            if(data != NULL)
            {
                if(i > 0 && (prev + 1) != *data)
                {
                    FIFO_PRINT_ERROR("assert (prev + 1) != *data\n");
                }
                prev = *data;
                s->sum += prev;
                ++s->countok;
                ++i;
            } else {
                ++s->countmiss;
                FIFO_THREAD_YIELD();
            }
            break;
        case QUEUE_STOP:
            return 0;
        default:
            FIFO_PRINT("fifo_fn_fix_pop_first return: %d\n", ret);
            ++s->counterr;
            break;
        }
    }
    return 0;
}

static void test_fifo_fn_fix_impl(fifo_head_fn_fix_t * head,
                                  thread_func_t adding, thread_func_t deleting,
                                  int *data_pool)
{
    assert(head && adding && deleting && data_pool);
    FIFO_THREAD_TYPE tadd, tdel;
    thread_adding_arg_t sadd = {0};
    thread_deleting_arg_t sdel = {0};

    sadd.head = head;
    sadd.data_pool = data_pool;
    sadd.add = rand() % params.num;
    sdel.head = head;

    uint64_t t = get_hrtime();
    int ret;

    if( (ret = FIFO_THREAD_CREATE(& tadd, adding, & sadd, "writer")) != 0)
    {
        FIFO_PRINT_ERROR("pthread_create error: %s (%d)\n", FIFO_STRERROR(ret), ret);
        return;
    }
    if( (ret = FIFO_THREAD_CREATE(& tdel, deleting, & sdel, "reader")) != 0)
    {
        FIFO_PRINT_ERROR("pthread_create error: %s (%d)\n", FIFO_STRERROR(ret), ret);
        return;
    }

    FIFO_THREAD_JOIN(tadd, NULL);
    FIFO_THREAD_JOIN(tdel, NULL);

    t = FIFO_TIMESTAMP_NS - t;
    FIFO_PRINT("elapsed time: %.4f sec.\n", ((double)t) / NSEC_PER_SEC);

    FIFO_PRINT("addsum = %" PRIi64 " delsum = %" PRIi64 "\n", sadd.sum, sdel.sum);
    FIFO_PRINT("acountok = %u dcountok = %u\n", sadd.countok, sdel.countok);
    FIFO_PRINT("acountmiss: %u acounterr: %u dcountmiss: %u dcounterr: %u\n",
                   sadd.countmiss, sadd.counterr, sdel.countmiss, sdel.counterr);
    FIFO_PRINT("FIFO is empty: %s\n", head->begin->next == NULL ? "yes" : "no");
    if(sadd.sum != sdel.sum || sadd.countok != sdel.countok)
    {
        FIFO_PRINT_ERROR("Test is faild\n");
    }
}

static int test_fn_head_init(fifo_head_fn_fix_t *head)
{
    assert(head);
    const size_t queue_size = (size_t) (params.pool_size == 0 ? params.num_node_per_thread : params.pool_size);
    FIFO_PRINT("%s(): max queue size = %" PRIuPTR "\n", __func__, queue_size);
    return fifo_fn_fix_init(head, queue_size);
}

#if defined(_WIN32) || defined(__linux__)
int main(int argc, char *argv[])
#else
int test_main(int argc, char *argv[])
#endif
{
    UNUSED(argc); UNUSED(argv);
    fifo_head_fn_fix_t head;
    int32_t *data_pool = NULL;
    params.num = 500;
    params.num_node_per_thread = NODE_SIZE;
    params.pool_size = POOL_SIZE;
    const size_t data_size = ((size_t)params.num_node_per_thread) * sizeof(int);

    if(test_fn_head_init(& head) != 0)
    {
        return 1;
    }

    do {
        if( (data_pool = FIFO_MALLOC(data_size)) == NULL)
        {
            FIFO_PRINT_ERROR("Not enough of memory for data pool\n");
            break;
        }

        FIFO_PRINT("data_pool size : %.2f (MB)\n", (double)data_size / (double)(1024 * 1024));

        for(uint32_t i = 0; i < TEST_COUNT; ++i)
        {
            FIFO_PRINT("iteration: %u\n", i);
            test_fifo_fn_fix_impl(& head, fn_adding, fn_deleting, data_pool);
        }
    } while(0);
    if(data_pool != NULL) FIFO_FREE(data_pool);
    fifo_fn_fix_destroy(& head);
}
