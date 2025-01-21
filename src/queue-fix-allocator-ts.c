#include "queue-fix-allocator-ts.h"
#include "queue-delays.h"
#include "queue-error.h"

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#define QUEUE_FA_IS_EMPTY(fa_head) 						(fa_head->begin->next == NULL)
#define QUEUE_FA_NODE_INIT(fa_node)                     (fa_node->next = NULL)

//void fix_allocator_add_ref(fix_allocator_head_t * head)
//{
//    atomic_fetch_add(& head->refcount, 1);
//}

/*
 * IN:
 * 		head	- fix allocator context
 * 		pool_size - max number of items into the fix allocator pool
 * 		node_size - size each node from fix allocator pool
 * RETURN:
 * 		0 - if success, else - error code: ERR_NOT_MEMORY
 */
int fix_allocator_init(fix_allocator_head_t * head, size_t pool_size, size_t node_size)
{
    assert(head && (pool_size > 0) && (node_size > 0));

    memset(head, 0, sizeof(*head));
    head->stop_flag = 1;
    int ret;
    int stepcount = 0;
    do {
        if( (ret = FIFO_MTX_INIT(& head->mbegin)) )
        {
            FIFO_PRINT_ERROR("mbegin mutex init error.\n");
            break;
        }
        ++stepcount;		// 1
        if( (ret = FIFO_MTX_INIT(& head->mend)) )
        {
            FIFO_PRINT_ERROR("mend mutex init error.\n");
            break;
        }
        ++stepcount;		// 2
        if( (ret = FIFO_CND_INIT(& head->cond)) )
        {
            FIFO_PRINT_ERROR("Condition variable init error.\n");
            break;
        }
        ++stepcount;		// 3

        head->node_fa_size = node_size >= sizeof(node_fa_t) ? node_size : sizeof(node_fa_t);
        head->node_fa_size = ((head->node_fa_size - 1) | (sizeof(void*) - 1)) + 1;          /* aligned node */
        head->queue_size = pool_size + 1;                                                   /* +1 for dummy node */
        head->memory_pool_size = head->queue_size * head->node_fa_size;
        head->memory_pool = FIFO_MALLOC(head->memory_pool_size);
        if(head->memory_pool == NULL)
        {
            FIFO_PRINT_ERROR("Not enough of memory.\n");
            ret = QUEUE_ERR_NOT_MEMORY;
            break;
        }
        memset(head->memory_pool, 0, head->memory_pool_size);
        FIFO_PRINT("memory pool is %" PRIuPTR " bytes.\n", head->memory_pool_size);
        ++stepcount;		// 4

        // filling queue
        head->begin = (node_fa_t*)head->memory_pool;
        node_fa_t * const end = (node_fa_t *)( ((uint8_t*)head->begin) + head->memory_pool_size - head->node_fa_size);
        for(node_fa_t * cur_node = head->begin; cur_node < end; cur_node = cur_node->next)
        {
            cur_node->next = (node_fa_t*)( ((uint8_t*)cur_node) + head->node_fa_size );
        }
        head->end = end;
        head->stop_flag = 0;
    } while(0);
    if(ret)
    {
        switch(stepcount)
        {
        case 4:
            FIFO_FREE(head->memory_pool);
            /*fall through*/
        case 3:
            FIFO_CND_DESTROY(& head->cond);
            /*fall through*/
        case 2:
            FIFO_MTX_DESTROY(& head->mend);
            /*fall through*/
        case 1:
            FIFO_MTX_DESTROY(& head->mbegin);
            /*fall through*/
        default:
            break;
        }
        memset(head, 0, sizeof(*head));
    }
    return ret;
}

static inline void fifo_fa_push_back_no_lock(fix_allocator_head_t * head, node_fa_t *node)
{
    head->end->next = node;
    head->end = node;
}

static inline node_fa_t* fifo_fa_remove_first_no_lock(fix_allocator_head_t * head)
{
    node_fa_t * const node = head->begin;
    head->begin = node->next;
    return node;
}

void fix_allocator_destroy(fix_allocator_head_t * head)
{
//    if(atomic_fetch_sub(& head->refcount, 1) > 1)
//    {
//        return;
//    }
    head->stop_flag = 1;
    FIFO_CND_DESTROY(& head->cond);
    FIFO_MTX_DESTROY(& head->mbegin);
    FIFO_MTX_DESTROY(& head->mend);
    if(head->memory_pool)
    {
        FIFO_FREE(head->memory_pool);
    }
    memset(head, 0, sizeof(*head));
}

void fix_allocator_free(fix_allocator_head_t * head, void * data)
{
    assert(head && data);
    if(head->stop_flag)
    {
        return;
    }
#ifdef NDEBUG
    if(     (data < head->memory_pool)
        ||   ((uint8_t*)data >= ((uint8_t*)head->memory_pool + head->memory_pool_size)))
    {
        FIFO_PRINT("Wrong data address: %p [%p; %p)\n", data, head->memory_pool,
                       (void*)((uint8_t*)head->memory_pool + head->memory_pool_size));
        return;
    }
#else
    assert(	((void*)data >= head->memory_pool) &&
            ((uint8_t*)data < ((uint8_t*)head->memory_pool + head->queue_size * head->node_fa_size)));
#endif

    node_fa_t *node = data;
    QUEUE_FA_NODE_INIT(node);
    FIFO_MTX_LOCK(& head->mend);
    fifo_fa_push_back_no_lock(head, node);
    FIFO_MTX_UNLOCK(& head->mend);
    FIFO_CND_SIGNAL(& head->cond);
}

void * fix_allocator_malloc(fix_allocator_head_t * head)
{
    assert(head);
    if(head->stop_flag)
    {
        return NULL;
    }

    struct timespec ts;
    node_fa_t *node = NULL;
    uint32_t count = 0;
    FIFO_MTX_LOCK(& head->mbegin);
    while(QUEUE_FA_IS_EMPTY(head))
    {
        if(head->stop_flag || count++ >= MAX_WAIT_FREE_NODE)
        {
            FIFO_MTX_UNLOCK(& head->mbegin);
            return NULL;
        }
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, CONDITION_DELAY_NANO);
        FIFO_CND_TIMEDWAIT(& head->cond, & head->mbegin, & ts);
    }
    node = fifo_fa_remove_first_no_lock(head);
    FIFO_MTX_UNLOCK(& head->mbegin);
    QUEUE_FA_NODE_INIT(node);
    return node;
}
