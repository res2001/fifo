#include "fifo-fake-node-fix.h"
#include <inttypes.h>
#include <assert.h>

#define FIFO_FNF_IS_EMPTY(head) 						(head->begin->next == NULL)
#define FIFO_FNF_IS_NOT_EMPTY(head) 					(head->begin->next != NULL)
#define FIFO_FNF_NODE_INIT(node)						do { node->data = NULL; node->next = NULL; } while(0)

static int  fifo_fn_fix_pool_init(fifo_head_fn_fix_t *head, size_t queue_size)
{
    assert(head && queue_size > 0);
    head->queue_size = queue_size + 2;           /* +2 for dummy node for allocator queue and data queue */
    const size_t memory_pool_size = head->queue_size * sizeof(node_fn_fix_t);
    head->pool = FIFO_MALLOC(memory_pool_size);
    if(head->pool == NULL)
    {
        FIFO_PRINT_ERROR("Not enough of memory.\n");
        return QUEUE_ERR_NOT_MEMORY;
    }
    memset(head->pool, 0, memory_pool_size);
    FIFO_PRINT("memory pool is %" PRIuPTR " bytes (%p).\n", memory_pool_size, (void*)head->pool);

    // filling pool queue
    head->abegin = head->pool;
    node_fn_fix_t * const end = head->abegin + head->queue_size - 1;
    for(node_fn_fix_t *cur_node = head->abegin; cur_node < end; cur_node = cur_node->next)
    {
        cur_node->next = cur_node + 1;
    }
    head->aend = end;
    return QUEUE_SUCCESS;
}

static void fifo_fn_fix_pool_destroy(fifo_head_fn_fix_t *head)
{
    assert(head);
    if(head->pool)
    {
        FIFO_FREE(head->pool);
        head->pool = NULL;
        head->abegin = head->aend = NULL;
    }
}

static inline void fifo_fn_fix_node_free(fifo_head_fn_fix_t *head, node_fn_fix_t *node)
{
    assert(head && head->pool && node);
#ifdef NDEBUG
    if((node < head->pool) || (node >= (head->pool + head->queue_size)))
    {
        FIFO_PRINT_ERROR("Wrong data address: %p [%p; %p)\n", (void*)node, (void*)head->pool, (void*)(head->pool + head->queue_size));
        return;
    }
#else
    assert((node >= head->pool) && (node < (head->pool + head->queue_size)));
#endif

    FIFO_FNF_NODE_INIT(node);
    node_fn_fix_t * const cur = head->aend;
    head->aend = node;
    cur->next = node;
}

static inline node_fn_fix_t* fifo_fn_fix_node_malloc(fifo_head_fn_fix_t *head)
{
    assert(head && head->pool);
    node_fn_fix_t * const node = head->abegin;
    if(node->next != NULL)  /* queue is not empty? */
    {
        head->abegin = node->next;
        node->next = NULL;
        return node;
    }
    return NULL;
}

int fifo_fn_fix_init(fifo_head_fn_fix_t *head, size_t queue_size)
{
    assert( head && queue_size > 0);
    memset(head, 0, sizeof(*head));
    int ret;
    if( (ret = fifo_fn_fix_pool_init(head, queue_size)) != QUEUE_SUCCESS)
    {
        return ret;
    }

    node_fn_fix_t *node = fifo_fn_fix_node_malloc(head);
    if(!node)
    {
        FIFO_PRINT_ERROR("Not enough of memory.\n");
        return QUEUE_ERR_NOT_MEMORY;
    }
    head->begin = node;
    head->end = node;
    return ret;
}

void fifo_fn_fix_release_all(fifo_head_fn_fix_t * head)
{
    head->stop_flag = 1;
}

void fifo_fn_fix_destroy(fifo_head_fn_fix_t * head)
{
    fifo_fn_fix_release_all(head);
    fifo_fn_fix_pool_destroy(head);
    memset(head, 0, sizeof(*head));
}

static inline void fifo_fn_fix_push_back_no_lock(fifo_head_fn_fix_t *head, node_fn_fix_t *new_node, void *new_data)
{
    /*
     * It is different from PODC96.
     * The implementation is taken from "C++ Concurrency in Action" Antony Williams
     */
    assert(head->end != NULL && head->end->next == NULL);
    node_fn_fix_t *cur = head->end;
    head->end = new_node;
    cur->data = new_data;
    cur->next = new_node;
}

static inline node_fn_fix_t* fifo_fn_fix_pop_first_no_lock(fifo_head_fn_fix_t *head)
{
    /* The check for an empty queue must be in the calling code. */
    node_fn_fix_t * const node = head->begin;
    assert(node != NULL && node->next != NULL);
    head->begin = node->next;
    return node;
}

int fifo_fn_fix_push_back(fifo_head_fn_fix_t *head, void *new_data)
{
    assert(head);

    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    node_fn_fix_t * const new_node = fifo_fn_fix_node_malloc(head);
    if(new_node != NULL)
    {
        fifo_fn_fix_push_back_no_lock(head, new_node, new_data);
        return QUEUE_SUCCESS;
    }

    return QUEUE_ERR_NOT_MEMORY;
}

int fifo_fn_fix_pop_first(fifo_head_fn_fix_t * head, void **data)
{
    assert(head && data);

    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    if(FIFO_FNF_IS_NOT_EMPTY(head))
    {
        node_fn_fix_t * const node = fifo_fn_fix_pop_first_no_lock(head);
        *data = node->data;
        fifo_fn_fix_node_free(head, node);
    }

    return QUEUE_SUCCESS;
}

int fifo_fn_fix_flush_step(fifo_head_fn_fix_t *head, void **data, int ret)
{
    assert(head && data);
    if(ret != QUEUE_SUCCESS && ret != QUEUE_AGAIN)
    {
        return ret;
    }

    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    if(FIFO_FNF_IS_NOT_EMPTY(head))
    {
        node_fn_fix_t * const node = fifo_fn_fix_pop_first_no_lock(head);
        *data = node->data;
        ret = node->next == NULL ? QUEUE_SUCCESS : QUEUE_AGAIN;
        fifo_fn_fix_node_free(head, node);
    } else {
        *data = NULL;
        ret = QUEUE_SUCCESS;
    }

    return ret;
}
