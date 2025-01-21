/*
 * Two-lock FIFO Queue implementation with built-in fixed node allocator without additional locks.
 * Thread safe FIFO Queue with finely-granular blocking and fake node.
 * Implementation of the "Two-Lock Concurrent Queue Algorithm" described in the
 * "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms",
 * Maged M. Michael, Michael L. Scott, PODC96
 * https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html#tlq
 */

#ifndef FIFO_TWO_LOCK_FIX_H_
#define FIFO_TWO_LOCK_FIX_H_
#include <fifo-common.h>

typedef struct node_tlq_fix_s node_tlq_fix_t;
struct node_tlq_fix_s {
    struct node_tlq_fix_s * next;
    void * data;
};

typedef struct {
    FIFO_MTX_DECLARE(mbegin);
    FIFO_MTX_DECLARE(mend);
    FIFO_CND_DECLARE(cond);
    node_tlq_fix_t *pool;
    size_t queue_size;
    node_tlq_fix_t * volatile begin, * volatile end;
    node_tlq_fix_t * volatile abegin, * volatile aend;
    volatile uint8_t stop_flag;
} fifo_head_tlq_fix_t;

int fifo_tlq_fix_init(fifo_head_tlq_fix_t *head, size_t queue_size);
void fifo_tlq_fix_destroy(fifo_head_tlq_fix_t *head);
void fifo_tlq_fix_release_all(fifo_head_tlq_fix_t *head);
int fifo_tlq_fix_wait_push_back(fifo_head_tlq_fix_t *head, void *new_data);
int fifo_tlq_fix_try_push_back(fifo_head_tlq_fix_t *head, void *new_data);
int fifo_tlq_fix_wait_pop_first(fifo_head_tlq_fix_t *head, void **data);
int fifo_tlq_fix_timedwait_pop_first(fifo_head_tlq_fix_t *head, void **data, uint64_t timeout_ns);
int fifo_tlq_fix_try_pop_first(fifo_head_tlq_fix_t *head, void **data);
int fifo_tlq_fix_flush_step(fifo_head_tlq_fix_t *head, void **data, int ret);

#endif /* FIFO_TWO_LOCK_FIX_H_ */
