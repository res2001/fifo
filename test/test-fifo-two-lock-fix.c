#include "test-fifo.h"
#include <fifo-two-lock-fix.h>
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

typedef struct
{
    fifo_head_tlq_fix_t * head;
    int64_t * data_pool;
    int64_t sum;
    int64_t add;
    uint32_t countok, counterr, countmiss;
    int num_thread;
} thread_adding_arg_t;

typedef struct
{
    fifo_head_tlq_fix_t * head;
    int64_t sum;
    uint32_t countok, countbusy, counterr, countmiss;
    int num_thread;
} thread_deleting_arg_t;

static atomic_bool stop = false;

static void* tlq_adding(void * arg)
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
        s->data_pool[i] = data;
        const int ret = fifo_tlq_fix_wait_push_back(s->head, s->data_pool + i);
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
static void* tlq_deleting_wait(void * arg)
{
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    int nt = s->num_thread;
    int64_t i = 0;
    int64_t * data;
    s->sum = 0;

    if(set_thread_affinity() != 0)
    {
        ++s->counterr;
        return (void*)-1;
    }

    while(true)
    {
        data = NULL;
        const int ret = fifo_tlq_fix_wait_pop_first(s->head, (void**)& data);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            if (!params.is_bench)
                ck_assert_ptr_nonnull(data);
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

static void* tlq_deleting_try(void * arg)
{
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    int64_t i = 0;
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
        const int ret = fifo_tlq_fix_try_pop_first(s->head, (void**)& data);
        switch(ret)
        {
        case QUEUE_SUCCESS:
            if(data != NULL)
            {
                s->sum += *data;
                ++s->countok;
                ++i;
                break;
            }
            if (atomic_load_explicit(& stop, memory_order_relaxed))
            {
                if (nt++ != params.delthread)
                    return 0;

                break;
            }
            ++s->countmiss;
            sched_yield();
            break;
        case QUEUE_ERR_TIMEDOUT:
            ++s->countbusy;
            break;
        case QUEUE_STOP:
            return 0;
        default:
            LOG_WRITE_DEBUG("fifo_tlq_fix_try_pop_first return: %d\n", ret);
            ++s->counterr;
            break;
        }
    }
    return 0;
}

static void test_fifo_tlq_fix_impl(const char * msg, fifo_head_tlq_fix_t * head,
                                   thread_func_t adding, thread_func_t deleting)
{
    LOG_WRITE_INFO("Test %s\n", msg);
    pthread_t * tadd = calloc((size_t)params.addthread, sizeof(pthread_t));
    pthread_t * tdel = calloc((size_t)params.delthread, sizeof(pthread_t));
    thread_adding_arg_t * sadd = calloc((size_t)params.addthread, sizeof(thread_adding_arg_t));
    thread_deleting_arg_t * sdel = calloc((size_t)params.delthread, sizeof(thread_deleting_arg_t));
    int64_t * data_pool = calloc((size_t)(params.num_node_per_thread * params.addthread), sizeof(int64_t));
    LOG_WRITE_INFO("%s: data_pool size : %.2f (MB)\n",
                   msg, (double)((size_t)(params.num_node_per_thread * params.addthread) * sizeof(int64_t)) / (double)(1024 * 1024));
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
    uint32_t acountok = 0, acounterr = 0, acountmiss = 0;
    uint32_t dcountok = 0, dcountbusy = 0, dcounterr = 0, dcountmiss = 0;
    LOG_WRITE_INFO("Wait stop add threads\n");
    for(i = 0; i < params.addthread; ++i)
    {
        pthread_join(tadd[i], NULL);
        addsum += sadd[i].sum;
        acountok += sadd[i].countok;
        acounterr += sadd[i].counterr;
        acountmiss += sadd[i].countmiss;
        LOG_WRITE_DEBUG("addsum[%d] = %" PRIi64 " Counters OK: %u miss: %u err: %u\n",
                       i, sadd[i].sum, sadd[i].countok, sadd[i].countmiss, sadd[i].counterr);
    }
    const uint64_t t1 = get_hrtime() - t;

    LOG_WRITE_INFO("Stop remover threads\n");
    atomic_store_explicit(& stop, true, memory_order_relaxed);
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
    LOG_WRITE_INFO("acountmiss: %u acounterr: %u dcountmiss: %u dcounterr: %u dcountbusy: %u\n",
                   acountmiss, acounterr, dcountmiss, dcounterr, dcountbusy);
    LOG_WRITE_INFO("FIFO is empty: %s\n", head->begin->next == NULL ? "yes" : "no");
    ck_assert_int_eq(addsum, delsum);
    ck_assert_uint_eq(acountok, dcountok);
}

static void test_tlq_head_init(fifo_head_tlq_fix_t *head)
{
    assert(head);
    const size_t queue_size = (size_t) (params.pool_size == 0 ? params.num_node_per_thread * params.addthread : params.pool_size);
    LOG_WRITE_INFO("%s(): max queue size = %" PRIuPTR "\n", __func__, queue_size);
    if(fifo_tlq_fix_init(head, queue_size))
    {
        ck_abort_msg("Queue initialization error.");
    }
}

START_TEST(test_fifo_tlq_fix_delwait)
{
    fifo_head_tlq_fix_t head;
    test_tlq_head_init(& head);
    test_fifo_tlq_fix_impl(__func__, & head, tlq_adding, tlq_deleting_wait);
    fifo_tlq_fix_destroy(& head);
}
END_TEST

START_TEST(test_fifo_tlq_fix_deltry)
{
    fifo_head_tlq_fix_t head;
    test_tlq_head_init(& head);
    test_fifo_tlq_fix_impl(__func__, & head, tlq_adding, tlq_deleting_try);
    fifo_tlq_fix_destroy(& head);
}
END_TEST

Suite* fifo_tlq_fix_suite(void)
{
    Suite *s = suite_create("fifo-tlq-fix");
    TCase *tc;
    tc = tcase_create("fifo-tlq-fix-delwait");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_fifo_tlq_fix_delwait);
    tcase_set_timeout(tc, 0);

    tc = tcase_create("fifo-tlq-fix-deltry");
    suite_add_tcase(s, tc);
    tcase_add_test(tc, test_fifo_tlq_fix_deltry);
    tcase_set_timeout(tc, 0);

    return s;
}
