/*
 * Two-lock FIFO Queue implementation with intrusive nodes.
 * Thread safe FIFO Queue with finely-granular blocking and fake node.
 * Implementation of the "Two-Lock Concurrent Queue Algorithm" described in the
 * "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms",
 * Maged M. Michael, Michael L. Scott, PODC96
 * https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html#tlq
 */

#ifndef FIFO_TWO_LOCK_INTR_H_
#define FIFO_TWO_LOCK_INTR_H_
#include <fifo-common.h>
#include <containerof.h>

typedef struct fifo_node_tlqi_s {
    struct fifo_node_tlqi_s * next;
} fifo_node_tlqi_t;

typedef struct {
    fifo_node_tlqi_t fake_node;
    FIFO_MTX_DECLARE(mbegin);
    FIFO_MTX_DECLARE(mend);
    FIFO_CND_DECLARE(cond);
    fifo_node_tlqi_t * volatile begin, * volatile end;
    volatile uint8_t stop_flag;
    uint32_t fake_in, fake_out;
} fifo_head_tlqi_t;

int fifo_tlqi_init(fifo_head_tlqi_t *head);
void fifo_tlqi_destroy(fifo_head_tlqi_t *head);
void fifo_tlqi_release_all(fifo_head_tlqi_t *head);
int fifo_tlqi_try_push_back(fifo_head_tlqi_t * head, fifo_node_tlqi_t * new_node);
int fifo_tlqi_wait_push_back(fifo_head_tlqi_t *head, fifo_node_tlqi_t *new_node);
int fifo_tlqi_wait_pop_first(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node);
int fifo_tlqi_timedwait_pop_first(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node, uint64_t timeout_ns);
int fifo_tlqi_try_pop_first(fifo_head_tlqi_t * head, fifo_node_tlqi_t **node);
int fifo_tlqi_flush_step(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node, int ret);

#endif /* FIFO_TWO_LOCK_INTR_H_ */
