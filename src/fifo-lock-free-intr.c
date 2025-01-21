#include "fifo-lock-free-intr.h"
#include <assert.h>

#define FIFO_TLQI_NODE_INIT(node)						do { node->next = NULL; } while(0)

void fifo_lfi_destroy(fifo_head_lfi_t * head)
{
    memset(head, 0, sizeof(*head));
}

int fifo_lfi_init(fifo_head_lfi_t *head)
{
    assert(head);
    memset(head, 0, sizeof(*head));
    head->begin = & head->fake_node;
    head->end = & head->fake_node;
    head->fake_in = 1;
    return QUEUE_SUCCESS;
}

void fifo_lfi_push_back(fifo_head_lfi_t *head, fifo_node_lfi_t *new_node)
{
    assert(head && new_node && head->end);
    FIFO_TLQI_NODE_INIT(new_node);
    fifo_node_lfi_t * prev = atomic_exchange_explicit(& head->end, new_node, memory_order_relaxed);
    prev->next = new_node;
    atomic_thread_fence(memory_order_release);
    // atomic_store_explicit(& prev->next, new_node, memory_order_release);
}

fifo_node_lfi_t* fifo_lfi_pop_first(fifo_head_lfi_t *head)
{
    assert(head && head->begin);
    fifo_node_lfi_t *node, *next;

    while(true)
    {
        node = head->begin;
        atomic_thread_fence(memory_order_acquire);
        next = node->next;
//        next = atomic_load_explicit(& node->next, memory_order_acquire);

        if (next != NULL)
        {
            head->begin = next;
            FIFO_TLQI_NODE_INIT(node);
            if (node != & head->fake_node)
                return node;
            ++head->fake_out;
        } else if (node != & head->fake_node) {
            fifo_lfi_push_back(head, & head->fake_node);
            ++head->fake_in;
        } else
            return NULL;
    }
}

bool fifo_lfi_is_empty(fifo_head_lfi_t *head)
{
    assert(head && head->begin);
    atomic_thread_fence(memory_order_acquire);
    return NULL == head->begin->next;
    // return NULL == atomic_load_explicit(& head->begin->next, memory_order_relaxed);
}
