/*
 * A simple intrusive queue with one shared lock based on a single-link list.
 */

#ifndef FIFO_LOCK_INTR_H_
#define FIFO_LOCK_INTR_H_
#include <fifo-common.h>
#include <containerof.h>

typedef struct fifo_node_li_s {
    struct fifo_node_li_s *next;
} fifo_node_li_t;

typedef struct {
    FIFO_MTX_DECLARE(mt);
    FIFO_CND_DECLARE(cond);
    fifo_node_li_t * volatile begin, * volatile end;
    volatile uint8_t stop_flag;
} fifo_head_li_t;

int fifo_li_init(fifo_head_li_t *head);
void fifo_li_destroy(fifo_head_li_t *head);
void fifo_li_release_all(fifo_head_li_t *head);
int fifo_li_try_push_back(fifo_head_li_t * head, fifo_node_li_t * new_node);
int fifo_li_wait_push_back(fifo_head_li_t *head, fifo_node_li_t *new_node);
int fifo_li_wait_pop_first(fifo_head_li_t *head, fifo_node_li_t **node);
int fifo_li_timedwait_pop_first(fifo_head_li_t *head, fifo_node_li_t **node, uint64_t timeout_ns);
int fifo_li_try_pop_first(fifo_head_li_t * head, fifo_node_li_t **node);
int fifo_li_flush_step(fifo_head_li_t *head, fifo_node_li_t **node, int ret);

#endif /* FIFO_LOCK_INTR_H_ */
