#include "test-fifo.h"
#include "set-thread-affinity.h"
#include <fifo-fake-node-fix.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>

typedef void* (*thread_func_t)(void*);

typedef struct
{
    fifo_head_fn_fix_t * head;
    int64_t * data_pool;
    int64_t sum;
    int64_t add;
    uint32_t countok, counterr, countmiss;
    int num_thread;
} thread_adding_arg_t;

typedef struct
{
    fifo_head_fn_fix_t * head;
    int64_t sum;
    uint32_t countok, countbusy, counterr, countmiss;
} thread_deleting_arg_t;

static void* fn_adding(void * arg)
{
    assert(arg);
    thread_adding_arg_t * s = (thread_adding_arg_t*) arg;
    int64_t i = 0;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(i < params.num_node_per_thread)
    {
        const int64_t data = i + s->add;
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
            sched_yield();
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
    int64_t i = 0;
    int64_t * data;
    int64_t prev;
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
                if(i > 0) ck_assert_int_eq(prev + 1, *data);
                prev = *data;
                s->sum += prev;
                ++s->countok;
                ++i;
            } else {
                ++s->countmiss;
                sched_yield();
            }
            break;
        case QUEUE_STOP:
            return 0;
        default:
            LOG_WRITE_DEBUG("fifo_fn_fix_pop_first return: %d\n", ret);
            ++s->counterr;
            break;
        }
    }
    return 0;
}

static void test_fifo_fn_fix_impl(const char * msg, fifo_head_fn_fix_t * head,
                                   thread_func_t adding, thread_func_t deleting)
{
    LOG_WRITE_INFO("Test %s\n", msg);
    pthread_t tadd, tdel;
    thread_adding_arg_t sadd = {0};
    thread_deleting_arg_t sdel = {0};
    int64_t * const data_pool = calloc((size_t)(params.num_node_per_thread), sizeof(int64_t));
    ck_assert_ptr_nonnull(data_pool);
    LOG_WRITE_INFO("%s: data_pool size : %.2f (MB)\n",
                   msg, (double)((size_t)params.num_node_per_thread * sizeof(int64_t)) / (double)(1024 * 1024));

    sadd.head = head;
    sadd.data_pool = data_pool;
    sadd.add = rand() % params.addthread;
    sadd.num_thread = 1;

    sdel.head = head;


    uint64_t t = get_hrtime();
    int ret;

    if( (ret = pthread_create(& tadd, NULL, adding, & sadd)) != 0)
    {
        LOG_WRITE_ERROR("pthread_create error: %s (%d)\n", FIFO_STRERROR(ret), ret);
        ck_assert_int_eq(ret, 0);
    }
    if( (ret = pthread_create(& tdel, NULL, deleting, & sdel)) != 0)
    {
        LOG_WRITE_ERROR("pthread_create error: %s (%d)\n", FIFO_STRERROR(ret), ret);
        ck_assert_int_eq(ret, 0);
    }

    LOG_WRITE_INFO("Wait stop threads\n");
    pthread_join(tadd, NULL);
    pthread_join(tdel, NULL);

    t = get_hrtime() - t;
    LOG_WRITE_INFO("%s: elapsed time: %.4f sec.\n", msg, ((double)t) / NANOSEC);

    free(data_pool);
    LOG_WRITE_INFO("addsum = %" PRIi64 " delsum = %" PRIi64 "\n", sadd.sum, sdel.sum);
    LOG_WRITE_INFO("acountok = %u dcountok = %u\n", sadd.countok, sdel.countok);
    LOG_WRITE_INFO("acountmiss: %u acounterr: %u dcountmiss: %u dcounterr: %u\n",
                   sadd.countmiss, sadd.counterr, sdel.countmiss, sdel.counterr);
    LOG_WRITE_INFO("FIFO is empty: %s\n", head->begin->next == NULL ? "yes" : "no");
    ck_assert_int_eq(sadd.sum, sdel.sum);
    ck_assert_uint_eq(sadd.countok, sdel.countok);
}

static void test_fn_head_init(fifo_head_fn_fix_t *head)
{
    assert(head);
    const size_t queue_size = (size_t) (params.pool_size == 0 ? params.num_node_per_thread : params.pool_size);
    LOG_WRITE_INFO("%s(): max queue size = %" PRIuPTR "\n", __func__, queue_size);
    if(fifo_fn_fix_init(head, queue_size))
    {
        ck_abort_msg("Queue initialization error.");
    }
}

START_TEST(test_fifo_fn_fix)
{
    fifo_head_fn_fix_t head;
    test_fn_head_init(& head);
    test_fifo_fn_fix_impl(__func__, & head, fn_adding, fn_deleting);
    fifo_fn_fix_destroy(& head);
}
END_TEST

Suite* fifo_fn_fix_suite(void)
{
    Suite *s = suite_create("fifo-fn-fix");
    TCase *tc;
    tc = tcase_create("fifo-fn-fix");
    tcase_add_test(tc, test_fifo_fn_fix);
    tcase_set_timeout(tc, 0);
    suite_add_tcase(s, tc);
    return s;
}
