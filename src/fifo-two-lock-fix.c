#include "fifo-two-lock-fix.h"
#include <inttypes.h>
#include <assert.h>

#define FIFO_TLQ_IS_EMPTY(head) 						(head->begin->next == NULL)
#define FIFO_TLQ_IS_NOT_EMPTY(head) 					(head->begin->next != NULL)
#define FIFO_TLQ_NODE_INIT(node)						do { node->data = NULL; node->next = NULL; } while(0)

static int  fifo_tlq_fix_pool_init(fifo_head_tlq_fix_t *head, size_t queue_size)
{
    assert(head && queue_size > 0);
    head->queue_size = queue_size + 2;           /* +2 for dummy node for allocator queue and data queue */
    const size_t memory_pool_size = head->queue_size * sizeof(node_tlq_fix_t);
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
    node_tlq_fix_t * const end = head->abegin + head->queue_size - 1;
    for(node_tlq_fix_t *cur_node = head->abegin; cur_node < end; cur_node = cur_node->next)
    {
        cur_node->next = cur_node + 1;
    }
    head->aend = end;
    return QUEUE_SUCCESS;
}

static void fifo_tlq_fix_pool_destroy(fifo_head_tlq_fix_t *head)
{
    assert(head);
    if(head->pool)
    {
        FIFO_FREE(head->pool);
        head->pool = NULL;
    }
}

static inline void fifo_tlq_fix_node_free(fifo_head_tlq_fix_t *head, node_tlq_fix_t *node)
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

    FIFO_TLQ_NODE_INIT(node);
    head->aend->next = node;
    head->aend = node;
}

static inline node_tlq_fix_t* fifo_tlq_fix_node_malloc(fifo_head_tlq_fix_t *head)
{
    assert(head && head->pool);
    node_tlq_fix_t * const node = head->abegin;
    if(node->next != NULL)  /* queue is not empty? */
    {
        head->abegin = node->next;
        node->next = NULL;
        return node;
    }
    return NULL;
}

int fifo_tlq_fix_init(fifo_head_tlq_fix_t *head, size_t queue_size)
{
    assert( head && queue_size > 0);
    memset(head, 0, sizeof(*head));
    int ret;
    if( (ret = fifo_tlq_fix_pool_init(head, queue_size)) != QUEUE_SUCCESS)
    {
        return ret;
    }

    node_tlq_fix_t *node = fifo_tlq_fix_node_malloc(head);
    if(!node)
    {
        return QUEUE_ERR_NOT_MEMORY;
    }
    head->begin = node;
    head->end = node;
    int stepcount = 0;
    do {
        if( (ret = FIFO_MTX_INIT(& head->mbegin)) )
        {
            FIFO_PRINT_ERROR("Mutex init error: %d - %s\n", ret, FIFO_STRERROR(ret));
            break;
        }
        ++stepcount;
        if( (ret = FIFO_MTX_INIT(& head->mend)) )
        {
            FIFO_PRINT_ERROR("Mutex init error: %d - %s\n", ret, FIFO_STRERROR(ret));
            break;
        }
        ++stepcount;
        if( (ret = FIFO_CND_INIT(& head->cond)) )
        {
            FIFO_PRINT_ERROR("Condition variable init error: %d - %s\n", ret, FIFO_STRERROR(ret));
            break;
        }
        ++stepcount;
    } while(0);
    if(ret)
    {
        switch(stepcount)
        {
        case 3:
            FIFO_CND_DESTROY(& head->cond);
            // fall through
        case 2:
            FIFO_MTX_DESTROY(& head->mend);
            // fall through
        case 1:
            FIFO_MTX_DESTROY(& head->mbegin);
            // fall through
        default:
            fifo_tlq_fix_pool_destroy(head);
            memset(head, 0, sizeof(*head));
            break;
        }
    }
    return ret;
}

void fifo_tlq_fix_release_all(fifo_head_tlq_fix_t * head)
{
    head->stop_flag = 1;
    FIFO_CND_BROADCAST(& head->cond);
}

static inline void fifo_tlq_fix_push_back_no_lock(fifo_head_tlq_fix_t *head, node_tlq_fix_t *new_node, void *new_data)
{
    /*
     * It is different from PODC96.
     * The implementation is taken from "C++ Concurrency in Action" Antony Williams
     */
    head->end->data = new_data;
    head->end->next = new_node;
    head->end = new_node;
}

static inline node_tlq_fix_t* fifo_tlq_fix_pop_first_no_lock(fifo_head_tlq_fix_t *head)
{
    /* The check for an empty queue must be in the calling code. */
    node_tlq_fix_t * const node = head->begin;
    assert(node->next != NULL);
    head->begin = node->next;
    return node;
}

void fifo_tlq_fix_destroy(fifo_head_tlq_fix_t * head)
{
    fifo_tlq_fix_release_all(head);
    FIFO_CND_DESTROY(& head->cond);
    FIFO_MTX_DESTROY(& head->mend);
    FIFO_MTX_DESTROY(& head->mbegin);
    fifo_tlq_fix_pool_destroy(head);
    memset(head, 0, sizeof(*head));
}

int fifo_tlq_fix_try_push_back(fifo_head_tlq_fix_t * head, void * new_data)
{
    assert(head);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    node_tlq_fix_t * new_node = NULL;
    int ret;
    if( (ret = FIFO_MTX_TRYLOCK(& head->mend)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR_BUSY or QUEUE_ERR */
        return ret;
    }

    if(!head->stop_flag)
    {
        new_node = fifo_tlq_fix_node_malloc(head);
        if(new_node == NULL)
        {
            FIFO_MTX_UNLOCK(& head->mend);
            return QUEUE_ERR_NOT_MEMORY;
        }
        fifo_tlq_fix_push_back_no_lock(head, new_node, new_data);
    }

    FIFO_MTX_UNLOCK(& head->mend);

    if(new_node != NULL)
    {
        FIFO_CND_SIGNAL(& head->cond);
        return QUEUE_SUCCESS;
    }
    return QUEUE_STOP;
}

int fifo_tlq_fix_wait_push_back(fifo_head_tlq_fix_t *head, void *new_data)
{
    assert(head);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    node_tlq_fix_t * new_node = NULL;
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mend)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    if(!head->stop_flag)
    {
        new_node = fifo_tlq_fix_node_malloc(head);
        if(new_node == NULL)
        {
            FIFO_MTX_UNLOCK(& head->mend);
            return QUEUE_ERR_NOT_MEMORY;
        }
        fifo_tlq_fix_push_back_no_lock(head, new_node, new_data);
    }

    FIFO_MTX_UNLOCK(& head->mend);

    if(new_node != NULL)
    {
        FIFO_CND_SIGNAL(& head->cond);
        return QUEUE_SUCCESS;
    }
    return QUEUE_STOP;
}

/* Infinite waiting until the event occurs or until the stop_flag is set */
int fifo_tlq_fix_wait_pop_first(fifo_head_tlq_fix_t *head, void **data)
{
    assert(head && data);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    node_tlq_fix_t *node = NULL;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    while(head->stop_flag || FIFO_TLQ_IS_EMPTY(head))
    {
        if(head->stop_flag || (ret != QUEUE_SUCCESS && ret != QUEUE_ERR_TIMEDOUT))
        {
            FIFO_MTX_UNLOCK(& head->mbegin);
            /* Returned QUEUE_STOP, QUEUE_ERR */
            return head->stop_flag ? QUEUE_STOP : ret;
        }
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, CONDITION_DELAY_NANO);
        ret = FIFO_CND_TIMEDWAIT(& head->cond, & head->mbegin, & ts);
    }

    node = fifo_tlq_fix_pop_first_no_lock(head);
    *data = node->data;
    fifo_tlq_fix_node_free(head, node);
    FIFO_MTX_UNLOCK(& head->mbegin);
    return QUEUE_SUCCESS;
}

/* Waiting until the event occurs, until a timeout expires, or until the stop_flag is set */
int fifo_tlq_fix_timedwait_pop_first(fifo_head_tlq_fix_t *head, void **data, uint64_t timeout_ns)
{
    assert(head && data);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    node_tlq_fix_t *node = NULL;
//    uint32_t count_wait = 0;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    if(FIFO_TLQ_IS_EMPTY(head))
    {
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, timeout_ns);
        while(FIFO_TLQ_IS_EMPTY(head))
        {
            if(ret != QUEUE_SUCCESS || head->stop_flag)
            {
                FIFO_MTX_UNLOCK(& head->mbegin);
                /* Returned QUEUE_STOP, QUEUE_ERR_TIMEDOUT, QUEUE_ERR */
                return head->stop_flag ? QUEUE_STOP : ret;
            }
            ret = FIFO_CND_TIMEDWAIT(& head->cond, & head->mbegin, & ts);
        }
    }

    node = fifo_tlq_fix_pop_first_no_lock(head);
    *data = node->data;
    fifo_tlq_fix_node_free(head, node);
    FIFO_MTX_UNLOCK(& head->mbegin);
    return QUEUE_SUCCESS;
}

int fifo_tlq_fix_try_pop_first(fifo_head_tlq_fix_t * head, void **data)
{
    assert(head && data);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    node_tlq_fix_t * node = NULL;
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR or QUEUE_ERR_BUSY */
        return ret;
    }

    if(!head->stop_flag && FIFO_TLQ_IS_NOT_EMPTY(head))
    {
        node = fifo_tlq_fix_pop_first_no_lock(head);
        *data = node->data;
        fifo_tlq_fix_node_free(head, node);
    }

    FIFO_MTX_UNLOCK(& head->mbegin);
    return QUEUE_SUCCESS;
}

int fifo_tlq_fix_flush_step(fifo_head_tlq_fix_t *head, void **data, int ret)
{
    assert(head && data);
    if(ret != QUEUE_SUCCESS && ret != QUEUE_AGAIN)
    {
        return ret;
    }
    if(head->stop_flag)
    {
        if(ret == QUEUE_AGAIN)
        {
            FIFO_MTX_UNLOCK(& head->mbegin);
        }
        return QUEUE_STOP;
    }
    node_tlq_fix_t *node = NULL;

    if(ret == QUEUE_SUCCESS)
    {
        if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
        {
            /* Returned QUEUE_ERR */
            return ret;
        }
    }

    if(FIFO_TLQ_IS_NOT_EMPTY(head))
    {
        node = fifo_tlq_fix_pop_first_no_lock(head);
        *data = node->data;
        ret = node->next == NULL ? QUEUE_SUCCESS : QUEUE_AGAIN;
        fifo_tlq_fix_node_free(head, node);
    } else {
        *data = NULL;
        ret = QUEUE_SUCCESS;
    }

    if(ret == QUEUE_SUCCESS)
    {
        FIFO_MTX_UNLOCK(& head->mbegin);
    }
    return ret;
}
