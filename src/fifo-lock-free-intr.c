#include "fifo-lock-free-intr.h"
#include <assert.h>

#define FIFO_LFI_NODE_INIT(node)						do { node->next = NULL; } while(0)

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
#ifdef FIFO_DEBUG
    head->fake_in = 1;
#endif
    head->use_fake = true;
    return QUEUE_SUCCESS;
}

void fifo_lfi_push_back(fifo_head_lfi_t *head, fifo_node_lfi_t *new_node)
{
    assert(head && new_node && head->end);
    // FIFO_LFI_NODE_INIT(new_node);
    atomic_store_explicit(& new_node->next, NULL, memory_order_release);
    fifo_node_lfi_t * prev = atomic_exchange_explicit(& head->end, new_node, memory_order_relaxed);
    atomic_store_explicit(& prev->next, new_node, memory_order_release);

}

fifo_node_lfi_t* fifo_lfi_pop_first(fifo_head_lfi_t *head)
{
    assert(head && head->begin);
    fifo_node_lfi_t *node = head->begin;
/*
    while(true)
    {
        fifo_node_lfi_t *next = atomic_load_explicit(& node->next, memory_order_acquire);

        if (node != & head->fake_node)
        {
            if (next == NULL)
            {
                if (head->use_fake)
                    return NULL;
#ifdef FIFO_DEBUG
                ++head->fake_in;
#endif
                head->use_fake = true;
                fifo_lfi_push_back(head, & head->fake_node);
                if ( (next = atomic_load_explicit(& node->next, memory_order_acquire)) == NULL)
                    return NULL;
                // while( (next = atomic_load_explicit(& node->next, memory_order_acquire)) == NULL)
                //     FIFO_THREAD_YIELD();
            }

            head->begin = next;
            FIFO_LFI_NODE_INIT(node);
            return node;
        }

        if (next == NULL)
            return NULL;

#ifdef FIFO_DEBUG
        ++head->fake_out;
#endif
        head->use_fake = false;
        head->begin = next;
        node = next;
    }
*/

    while(true)
    {
        fifo_node_lfi_t *next = atomic_load_explicit(& node->next, memory_order_acquire);

        if (next != NULL)
        {
            head->begin = next;
            FIFO_LFI_NODE_INIT(node);
            if (node != & head->fake_node)
            {
                return node;
            }
            node = next;
            head->use_fake = false;
#ifdef FIFO_DEBUG
            ++head->fake_out;
#endif

        } else if (node != & head->fake_node && !head->use_fake) {
            fifo_lfi_push_back(head, & head->fake_node);
            head->use_fake = true;
#ifdef FIFO_DEBUG
            ++head->fake_in;
#endif
        } else {
            return NULL;
        }
    }

}

bool fifo_lfi_is_empty(fifo_head_lfi_t *head)
{
    assert(head && head->begin);
    atomic_thread_fence(memory_order_acquire);
    return head->begin == & head->fake_node && NULL == atomic_load_explicit(& head->begin->next, memory_order_relaxed);
}
