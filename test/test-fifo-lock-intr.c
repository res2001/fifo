#include "test-fifo.h"
#include <fifo-lock-intr.h>
#include <stdbool.h>
#include <stdatomic.h>

#if 0
#include "set-thread-affinity.h"
#else
#define set_thread_affinity()       0
#endif

#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>

typedef void * (*thread_func_t)(void*);

typedef struct {
    fifo_node_li_t node;
    int64_t data;
} test_data_t;

typedef struct
{
    fifo_head_li_t * head;
    test_data_t * data_pool;
    int64_t sum;
    int64_t add;
    uint32_t countok, counterr;
    int num_thread;
} thread_adding_arg_t;

typedef struct
{
    fifo_head_li_t * head;
    int64_t sum;
    uint32_t countok, countbusy, counterr, countmiss;
    int num_thread;
} thread_deleting_arg_t;

static atomic_bool stop = false;

static void* li_adding_wait(void * arg)
{
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
        s->data_pool[i].data = data;
        const int ret = fifo_li_wait_push_back(s->head, & s->data_pool[i].node);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            s->sum += data;
            ++s->countok;
            ++i;
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

/*
static void* li_deleting_wait(void * arg)
{
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    int64_t i = 0;
    fifo_node_li_t *node;
    int64_t * data;
    int nt = s->num_thread;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(true)
    {
        data = NULL;
        const int ret = fifo_li_wait_pop_first(s->head, & node);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            assert(data != NULL);
            s->sum += *data;
            ++s->countok;
            ++i;
            break;
        case QUEUE_ERR_TIMEDOUT:
            if (atomic_load_explicit(& stop, memory_order_relaxed))
            {
                if (nt < params.delthread)
                    return 0;

                if (++nt == (params.delthread + 10))
                    return 0;

                LOG_WRITE_INFO("Last tread timed out\n");
            }
            ++s->countbusy;
            break;
        case QUEUE_STOP:
            return 0;
        default:
            LOG_WRITE_DEBUG("fifo_tlq_fix_wait_pop_first return: %d\n", ret);
            ++s->counterr;
            break;
        }
    }
    return 0;
}
*/
static void* li_deleting_try(void * arg)
{
    ck_assert_ptr_nonnull(arg);
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    fifo_node_li_t *node;
    test_data_t *data;
    int nt = s->num_thread;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(true)
    {
        const int ret = fifo_li_try_pop_first(s->head, & node);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            if (!params.is_bench)
            {
                ck_assert_ptr_nonnull(node);
                ck_assert_ptr_null(node->next);
            }
            data = container_of(node, test_data_t, node);
            s->sum += data->data;
            ++s->countok;
            break;
        case QUEUE_EMPTY:
            if (atomic_load_explicit(& stop, memory_order_relaxed))
            {
                if (nt != params.delthread)
                    return 0;

                ++nt;
            }
            ++s->countmiss;
            sched_yield();
            break;
        case QUEUE_STOP:
            return 0;
        default:
            LOG_WRITE_ERROR("fifo_li_try_pop_first return: %d\n", ret);
            ck_abort();
        }
    }
    return 0;
}

static void test_fifo_li_impl(const char * msg, fifo_head_li_t * head,
                                   thread_func_t adding, thread_func_t deleting)
{
    LOG_WRITE_INFO("Test %s\n", msg);
    pthread_t * tadd = calloc((size_t)params.addthread, sizeof(pthread_t));
    pthread_t * tdel = calloc((size_t)params.delthread, sizeof(pthread_t));
    thread_adding_arg_t * sadd = calloc((size_t)params.addthread, sizeof(thread_adding_arg_t));
    thread_deleting_arg_t * sdel = calloc((size_t)params.delthread, sizeof(thread_deleting_arg_t));
    test_data_t * data_pool = calloc((size_t)(params.num_node_per_thread * params.addthread), sizeof(*data_pool));
    LOG_WRITE_INFO("%s: data_pool size : %.2f (MB)\n",
                   msg, (double)((size_t)(params.num_node_per_thread * params.addthread) * sizeof(*data_pool)) / (double)(1024 * 1024));

    if(!data_pool || !tadd || !tdel || !sadd || !sdel)
    {
        if(data_pool)	free(data_pool);
        if(tadd)		free(tadd);
        if(tdel)		free(tdel);
        if(sadd)		free(sadd);
        if(sdel)		free(sdel);
        ck_assert_ptr_nonnull(data_pool);
        ck_assert_ptr_nonnull(tadd);
        ck_assert_ptr_nonnull(tdel);
        ck_assert_ptr_nonnull(sadd);
        ck_assert_ptr_nonnull(sdel);
        return;
    }

    int i, j;
    const int end = params.addthread <= params.delthread ? params.addthread : params.delthread;

    for(i = 0; i < params.addthread; ++i)
    {
        sadd[i].head = head;
        sadd[i].data_pool = data_pool + i * params.num_node_per_thread;
        sadd[i].sum = 0;
        sadd[i].add = rand() % (params.addthread * i + 10);
        sadd[i].num_thread = i + 1;
    }
    for(i = 0; i < params.delthread; ++i)
    {
        sdel[i].head = head;
        sdel[i].sum = 0;
        sdel[i].num_thread = i + 1;
    }

    const uint64_t t = get_hrtime();

    for(i = 0; i < end; ++i)
    {
        pthread_create(tdel + i, NULL, deleting, sdel + i);
        pthread_create(tadd + i, NULL, adding, sadd + i);
    }
    j = i;
    for(; i < params.addthread; ++i)
    {
        pthread_create(tadd + i, NULL,  adding, sadd + i);
    }
    for(; j < params.delthread; ++j)
    {
        pthread_create(tdel + j, NULL, deleting, sdel + j);
    }

    int64_t addsum = 0;
    int64_t delsum = 0;
    uint32_t acountok = 0, acounterr = 0;
    uint32_t dcountok = 0, dcountbusy = 0, dcounterr = 0, dcountmiss = 0;
    LOG_WRITE_INFO("Wait stop add threads\n");
    for(i = 0; i < params.addthread; ++i)
    {
        pthread_join(tadd[i], NULL);
        addsum += sadd[i].sum;
        acountok += sadd[i].countok;
        acounterr += sadd[i].counterr;
        LOG_WRITE_DEBUG("addsum[%d] = %" PRIi64 " Counters OK: %u err: %u\n",
                       i, sadd[i].sum, sadd[i].countok, sadd[i].counterr);
    }
    const uint64_t t1 = get_hrtime() - t;

    atomic_store_explicit(& stop, true, memory_order_relaxed);
    LOG_WRITE_INFO("Stop remover threads\n");
    // fifo_tlq_fix_release_all(head);

    for(i = 0; i < params.delthread; ++i)
    {
        pthread_join(tdel[i], NULL);
        delsum += sdel[i].sum;
        dcountok += sdel[i].countok;
        dcounterr += sdel[i].counterr;
        dcountbusy += sdel[i].countbusy;
        dcountmiss += sdel[i].countmiss;
        LOG_WRITE_DEBUG("delsum[%d] = %" PRIi64 " Counters OK: %u miss: %u busy: %u err: %u\n",
                       i, sdel[i].sum, sdel[i].countok, sdel[i].countmiss, sdel[i].countbusy, sdel[i].counterr);
    }

    const uint64_t t2 = get_hrtime() - t;
    LOG_WRITE_NOTICE("Elapsed time for the add threads: %.4f sec.\n", ((double)t1) / NANOSEC);
    LOG_WRITE_NOTICE("All elapsed time: %.4f sec.\n", ((double)t2) / NANOSEC);

    free(data_pool);
    free(tadd);
    free(tdel);
    free(sadd);
    free(sdel);
    LOG_WRITE_INFO("addsum = %" PRIi64 " delsum = %" PRIi64 "\n", addsum, delsum);
    LOG_WRITE_INFO("acountok = %u dcountok = %u\n", acountok, dcountok);
    LOG_WRITE_INFO("acounterr: %u dcountmiss: %u dcounterr: %u dcountbusy: %u\n",
                   acounterr, dcountmiss, dcounterr, dcountbusy);
    LOG_WRITE_INFO("FIFO is empty: %s\n", head->begin == NULL ? "yes" : "no");
    ck_assert_int_eq(addsum, delsum);
    ck_assert_uint_eq(acountok, dcountok);
}
/*
START_TEST(test_fifo_li_delwait)
{
    fifo_head_li_t head;
    ck_assert_int_eq(fifo_li_init(&head), 0);
    test_fifo_li_impl(__func__, & head, li_adding, li_deleting_wait);
    fifo_tlq_fix_destroy(& head);
}
END_TEST
*/
START_TEST(test_fifo_li_deltry)
{
    fifo_head_li_t head;
    ck_assert_int_eq(fifo_li_init(&head), 0);
    test_fifo_li_impl(__func__, & head, li_adding_wait, li_deleting_try);
    fifo_li_destroy(& head);
}
END_TEST

Suite* fifo_li_suite(void)
{
    Suite *s = suite_create("fifo-li");
    TCase *tc;
/*    
    tc = tcase_create("fifo-li-delwait");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_fifo_li_delwait);
    tcase_set_timeout(tc, 0);
*/
    tc = tcase_create("fifo-li-deltry");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_fifo_li_deltry);
    tcase_set_timeout(tc, 0);

    return s;
}
