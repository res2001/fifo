/*
 * Implementation Lock-Free FIFO Queue with many enqueuer and one dequeuer with intrusive nodes.
 * Lock-Free thread safe FIFO Queue with fake node.
 * Implementation of the "Two-Lock Concurrent Queue Algorithm" described in the
 * "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms",
 * Maged M. Michael, Michael L. Scott, PODC96
 * https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html#tlq
 */
#ifndef FIFO_LOCK_FREE_INTR_H_
#define FIFO_LOCK_FREE_INTR_H_
#include <fifo-common.h>
#include <containerof.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct fifo_node_lfi_s {
    struct fifo_node_lfi_s * _Atomic next;
} fifo_node_lfi_t;

typedef struct {
    struct fifo_node_lfi_s fake_node;
    struct fifo_node_lfi_s *begin;
    struct fifo_node_lfi_s * _Atomic end;
    bool use_fake;
#ifdef FIFO_DEBUG
    uint32_t fake_in, fake_out;
#endif
} fifo_head_lfi_t;

int fifo_lfi_init(fifo_head_lfi_t *head);
void fifo_lfi_destroy(fifo_head_lfi_t *head);
void fifo_lfi_push_back(fifo_head_lfi_t *head, fifo_node_lfi_t *new_node);
fifo_node_lfi_t* fifo_lfi_pop_first(fifo_head_lfi_t *head);
bool fifo_lfi_is_empty(fifo_head_lfi_t *head);

#endif /* FIFO_LOCK_FREE_INTR_H_ */
