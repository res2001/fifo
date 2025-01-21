/*
 * FIFO Queue implementation with built-in fixed node allocator.
 * Thread-safe lock-free FIFO queue with fake node for 1 CPU and 1 reader and 1 writer.
 */

#ifndef FIFO_FAKE_NODE_FIX_H_
#define FIFO_FAKE_NODE_FIX_H_
#include <fifo-common.h>

struct node_fn_fix_s {
    struct node_fn_fix_s * volatile next;
    void * data;
};
typedef struct node_fn_fix_s node_fn_fix_t;

typedef struct {
    node_fn_fix_t *pool;
    size_t queue_size;
    node_fn_fix_t * volatile begin, * volatile end;
    node_fn_fix_t * volatile abegin, * volatile aend;
    volatile uint8_t stop_flag;
} fifo_head_fn_fix_t;

int fifo_fn_fix_init(fifo_head_fn_fix_t *head, size_t queue_size);
void fifo_fn_fix_destroy(fifo_head_fn_fix_t * head);
void fifo_fn_fix_release_all(fifo_head_fn_fix_t * head);
int fifo_fn_fix_push_back(fifo_head_fn_fix_t *head, void *new_data);
int fifo_fn_fix_pop_first(fifo_head_fn_fix_t * head, void **data);
int fifo_fn_fix_flush_step(fifo_head_fn_fix_t *head, void **data, int ret);

#endif /* FIFO_FAKE_NODE_FIX_H_ */
