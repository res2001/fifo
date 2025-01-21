/*
 *  The fixed memory allocator for thread safe queue
 */

#ifndef QUEUE_FIX_ALLOCATOR_TS_H_
#define QUEUE_FIX_ALLOCATOR_TS_H_
#include <fifo-common.h>
//#include <stdatomic.h>

typedef struct node_fa_s node_fa_t;
struct node_fa_s {
    struct node_fa_s * next;
    // uint8_t data[head->data_size]
};

typedef struct {
    FIFO_MTX_DECLARE(mbegin);
    FIFO_MTX_DECLARE(mend);
    FIFO_CND_DECLARE(cond);
    size_t memory_pool_size, queue_size, node_fa_size;
    void * memory_pool;
    node_fa_t * volatile begin;
    node_fa_t * volatile end;
//    atomic_int refcount;
    volatile uint8_t stop_flag;
} fix_allocator_head_t;

int fix_allocator_init(fix_allocator_head_t * head, size_t queue_size, size_t node_size);
//void fix_allocator_add_ref(fix_allocator_head_t * head);
void fix_allocator_destroy(fix_allocator_head_t * head);
void* fix_allocator_malloc(fix_allocator_head_t * head);
void fix_allocator_free(fix_allocator_head_t * head, void * data);

#endif /* QUEUE_FIX_ALLOCATOR_TS_H_ */
