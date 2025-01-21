#include "test-fifo.h"
#include <fifo-lock-free-intr.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>

typedef void* (*thread_func_t)(void*);

typedef struct {
    fifo_node_lfi_t node;
    int64_t data;
} test_data_t;

typedef struct {
    fifo_head_lfi_t * head;
    test_data_t * data_pool;
    int64_t sum;
    int64_t add;
    uint32_t countok;
    int num_thread;
} thread_adding_arg_t;

typedef struct {
    fifo_head_lfi_t * head;
    int64_t sum;
    uint32_t countok, countmiss;
    atomic_bool stop;
} thread_deleting_arg_t;

static void* ft_adding(void * arg)
{
    assert(arg);
    thread_adding_arg_t * s = (thread_adding_arg_t*) arg;
    s->sum = 0;

    for(int64_t i = 0; i < params.num_node_per_thread; ++i)
    {
        const int64_t data = i + s->add;
        s->data_pool[i].data = data;
        fifo_lfi_push_back(s->head, & s->data_pool[i].node);
        s->sum += data;
        ++s->countok;
    }
    return 0;
}

static void* ft_deleting(void * arg)
{
    assert(arg);
    thread_deleting_arg_t * s = (thread_deleting_arg_t*) arg;
    fifo_node_lfi_t *node;
    test_data_t * data;
    uint32_t stop_count = 0;
    s->sum = 0;
    
    while(true)
    {
        node = fifo_lfi_pop_first(s->head);

        if(node != NULL)
        {
            ck_assert_ptr_null(node->next);
            data = container_of(node, test_data_t, node);
            s->sum += data->data;
            ++s->countok;
        } else if (atomic_load_explicit(& s->stop, memory_order_relaxed)) {
            if (stop_count++ > 0)
                return 0;
        } else {
            ++s->countmiss;
            // sched_yield();
        }
    }
    return 0;
}

/*
static bool check_fifo(const fifo_head_tlqi_t * head)
{
    fifo_node_tlqi_t *next = head->begin;
    fifo_node_tlqi_t *node;
    int64_t sum = 0;
    uint32_t count = 0;
    test_data_t *data;
    ck_assert_ptr_nonnull(next);
    while(next != NULL)
    {
        node = next;
        if (node != & head->fake_node)
        {
            data = container_of(node, test_data_t, node);
            sum += data->data;
            ++count;
        }
        next = node->next;
    }
    ck_assert_ptr_eq(head->end, node);
    LOG_WRITE_INFO("Check FIFO: addsum: %" PRIi64 " Add count items: %u\n", sum, count);
    return true;
}
*/

static void test_fifo_tlqi_impl(const char * msg, fifo_head_lfi_t * head,
                                thread_func_t adding, thread_func_t deleting)
{
    LOG_WRITE_INFO("Test %s\n", msg);

    pthread_t *tadd = calloc((size_t)params.addthread, sizeof(pthread_t));
    pthread_t tdel;
    thread_adding_arg_t *sadd = calloc((size_t)params.addthread, sizeof(thread_adding_arg_t));
    thread_deleting_arg_t sdel = {0};
    test_data_t * data_pool = calloc((size_t)(params.num_node_per_thread * params.addthread), sizeof(*data_pool));
    LOG_WRITE_INFO("%s: data_pool size : %.2f (MB)\n",
                   msg, (double)((size_t)(params.num_node_per_thread * params.addthread) * sizeof(*data_pool)) / (double)(1024 * 1024));

    if(!data_pool || !tadd || !sadd)
    {
        if(data_pool)   free(data_pool);
        if(tadd)		free(tadd);
        if(sadd)		free(sadd);
        ck_assert_ptr_nonnull(data_pool);
        ck_assert_ptr_nonnull(tadd);
        ck_assert_ptr_nonnull(sadd);
        return;
    }

    int i;
    for(i = 0; i < params.addthread; ++i)
    {
        sadd[i].head = head;
        sadd[i].data_pool = data_pool + i * params.num_node_per_thread;
        sadd[i].sum = 0;
        sadd[i].add = rand() % (params.addthread * i + 10);
        sadd[i].num_thread = i + 1;
    }

    sdel.head = head;

    uint64_t t = get_hrtime();

    ck_assert_int_eq(pthread_create(& tdel, NULL, deleting, & sdel), 0);
    for(i = 0; i < params.addthread; ++i)
        ck_assert_int_eq(pthread_create(tadd + i, NULL, adding, sadd + i), 0);

    int64_t addsum = 0;
    uint32_t acountok = 0;
    LOG_WRITE_INFO("Wait stop add threads\n");
    for(i = 0; i < params.addthread; ++i)
    {
        pthread_join(tadd[i], NULL);
        addsum += sadd[i].sum;
        acountok += sadd[i].countok;
        LOG_WRITE_DEBUG("addsum[%d] = %" PRIi64 " Counters OK: %u\n",
                       i, sadd[i].sum, sadd[i].countok);
    }
    // check_fifo(head);

    LOG_WRITE_INFO("Stop remover thread\n");
    atomic_store_explicit(& sdel.stop, true, memory_order_relaxed);
    pthread_join(tdel, NULL);
    LOG_WRITE_DEBUG("delsum[%d] = %" PRIi64 " Counters OK: %u miss: %u\n",
                    i, sdel.sum, sdel.countok, sdel.countmiss);

    t = get_hrtime() - t;
    LOG_WRITE_NOTICE("%s: elapsed time: %.4f sec.\n", msg, ((double)t) / NANOSEC);

    LOG_WRITE_INFO("addsum = %" PRIi64 " delsum = %" PRIi64 "\n", addsum, sdel.sum);
    LOG_WRITE_INFO("acountok = %u dcountok = %u dcountmiss: %u\n", acountok, sdel.countok, sdel.countmiss);
    LOG_WRITE_INFO("FIFO is empty: %s\n", fifo_lfi_is_empty(head) ? "yes" : "no");
    LOG_WRITE_INFO("FIFO fake push: %u fake pop: %u\n", head->fake_in, head->fake_out);
    ck_assert_int_eq(addsum, sdel.sum);
    ck_assert_uint_eq(acountok, sdel.countok);
    free(data_pool);
    free(sadd);
    free(tadd);
}

START_TEST(test_fifo_tlqi)
{
    fifo_head_lfi_t head;
    fifo_lfi_init(& head);
    test_fifo_tlqi_impl(__func__, & head, ft_adding, ft_deleting);
    fifo_lfi_destroy(& head);
}
END_TEST

Suite* fifo_tlqi_suite(void)
{
    Suite *s = suite_create("fifo-tlqi");
    TCase *tc;
    tc = tcase_create("fifo-tlqi");
    tcase_add_test(tc, test_fifo_tlqi);
    tcase_set_timeout(tc, 0);
    suite_add_tcase(s, tc);
    return s;
}
