#include "test-fifo.h"
#include <queue-fix-allocator-ts.h>

#include <check.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>

#define MAX_ALLOC_NUMBER			1000

typedef struct
{
    fix_allocator_head_t * head;
    uint32_t num_thread;
} thread_context_t;

static void* thread_func(void * arg)
{
    thread_context_t * s = (thread_context_t*) arg;
    uint64_t * ptr;
    for(int64_t i = 0; i < params.num_node_per_thread; ++i)
    {
        LOG_WRITE_DEBUG("Thread %d before alloc\n", s->num_thread);
        ptr = (uint64_t*)fix_allocator_malloc(s->head);
        if(!ptr)
        {
            LOG_WRITE_ERROR("thread_func(): Out of emory.\n");
            break;
        }
        *ptr = (uint64_t)i;
        LOG_WRITE_DEBUG("Thread %d alloc %" PRIi64 " data write: %" PRIu64 "\n", s->num_thread, i, *ptr);
        fix_allocator_free(s->head, ptr);
        LOG_WRITE_DEBUG("Thread %d free %" PRIi64 "\n", s->num_thread, i);
    }
    return 0;
}

static void* thread_std_alloc_func(void * arg)
{
    const uint32_t num_thread = (uint32_t)(uint64_t) arg;
    uint64_t * ptr;
    for(int64_t i = 0; i < params.num_node_per_thread; ++i)
    {
        ptr = (uint64_t*)malloc(sizeof(uint64_t));
        if(!ptr)
        {
            LOG_WRITE_ERROR("thread_std_alloc_func(): Std out of emory.\n");
            break;
        }
        *ptr = (uint64_t)i;
        LOG_WRITE_DEBUG("thread_std_alloc_func(): Thread %d alloc %" PRIi64 " data write: %" PRIu64 "\n", num_thread, i, *ptr);
        free(ptr);
        LOG_WRITE_DEBUG("thread_std_alloc_func(): Thread %d free %" PRIi64 "\n", num_thread, i);
    }
    return 0;
}

START_TEST(test_std_allocator)
{
    LOG_WRITE_INFO("Test test_std_allocator()\n");
    LOG_WRITE_INFO("test_std_allocator(): allocator pool size: all memory\talloc/free thread: %d\titeration: %d\n",
            params.addthread, params.num_node_per_thread);
    pthread_t * tadd = calloc((size_t)params.addthread, sizeof(pthread_t));
    if(!tadd)
    {
        if(tadd)		free(tadd);
        ck_abort_msg("Error allocation memory.");
        return;
    }
    int i;
    uint64_t t = get_hrtime();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
    for(i = 0; i < params.addthread; ++i)
    {
        pthread_create(tadd + i, NULL, thread_std_alloc_func, (void*)(uint64_t)(i + 1));
    }
#pragma GCC diagnostic pop

    for(i = 0; i < params.addthread; ++i)
    {
        pthread_join(tadd[i], NULL);
    }

    t = get_hrtime() - t;
    LOG_WRITE_INFO("test_std_allocator(): test elapsed time: %.6f sec.\n", ((double)t) / NANOSEC);
    free(tadd);
}
END_TEST

START_TEST(test_queue_fix_allocator)
{
    LOG_WRITE_INFO("Test test_queue_fix_allocator()\n");
    LOG_WRITE_INFO("test_queue_fix_allocator(): allocator pool size: %d\talloc/free thread: %d\titeration: %d\n",
            params.pool_size, params.addthread, params.num_node_per_thread);
    fix_allocator_head_t alloc_head;
    uint64_t t = get_hrtime();
    if(fix_allocator_init(& alloc_head, (size_t)params.pool_size, sizeof(int64_t)))
    {
        ck_abort_msg("Allocator initialization error.");
    }
    t = get_hrtime() - t;
    LOG_WRITE_INFO("test_queue_fix_allocator(): fix_allocator_init elapsed time: %.6f sec.\n", ((double)t) / NANOSEC);

    pthread_t * tadd = calloc((size_t)params.addthread, sizeof(pthread_t));
    thread_context_t * sadd = calloc((size_t)params.addthread, sizeof(thread_context_t));
    if(!tadd || !sadd)
    {
        if(tadd)		free(tadd);
        if(sadd)		free(sadd);
        ck_abort_msg("Error allocation memory.");
        return;
    }
    int i;
    for(i = 0; i < params.addthread; ++i)
    {
        sadd[i].head = & alloc_head;
        sadd[i].num_thread = (uint32_t)i + 1;
    }

    t = get_hrtime();

    for(i = 0; i < params.addthread; ++i)
    {
        pthread_create(tadd + i, NULL, thread_func, sadd + i);
    }
    for(i = 0; i < params.addthread; ++i)
    {
        pthread_join(tadd[i], NULL);
    }

    t = get_hrtime() - t;
    LOG_WRITE_INFO("test_queue_fix_allocator(): test elapsed time: %.6f sec.\n", ((double)t) / NANOSEC);

    fix_allocator_destroy(& alloc_head);
    free(tadd);
    free(sadd);
}
END_TEST

Suite* fix_allocator_suite(void)
{
    Suite *s;
    TCase *tc;
    s = suite_create("Fix allocator suite");

    /* Core test case */
    tc = tcase_create("Core");
//    tcase_add_checked_fixture(tc_core, setup, teardown);
    tcase_add_test(tc, test_std_allocator);
    tcase_add_test(tc, test_queue_fix_allocator);
    suite_add_tcase(s, tc);
    return s;
}
