#include "fifo-lock-intr.h"
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#define FIFO_LI_IS_EMPTY(head)                      ((head)->begin == NULL)
#define FIFO_LI_NODE_INIT(node)						do { node->next = NULL; } while(0)

int fifo_li_init(fifo_head_li_t *head)
{
    assert( head);
    memset(head, 0, sizeof(*head));

    int stepcount = 0;
    int ret;
    do {
        if( (ret = FIFO_MTX_INIT(& head->mt)) )
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

        return 0;
    } while(0);

    switch(stepcount)
    {
    case 2:
        FIFO_CND_DESTROY(& head->cond);
        // fall through
    case 1:
        FIFO_MTX_DESTROY(& head->mt);
        // fall through
    default:
        memset(head, 0, sizeof(*head));
        break;
    }
    return ret;
}

void fifo_li_release_all(fifo_head_li_t * head)
{
    FIFO_MTX_LOCK(& head->mt);
    head->stop_flag = 1;
    FIFO_MTX_UNLOCK(& head->mt);
    FIFO_CND_BROADCAST(& head->cond);
}

void fifo_li_destroy(fifo_head_li_t * head)
{
    fifo_li_release_all(head);
    FIFO_CND_DESTROY(& head->cond);
    FIFO_MTX_DESTROY(& head->mt);
    memset(head, 0, sizeof(*head));
}

static inline void fifo_li_push_back_no_lock(fifo_head_li_t *head, fifo_node_li_t *new_node)
{
    if (head->end != NULL)
        head->end->next = new_node;
    else
        head->begin = new_node;
    head->end = new_node;
}

int fifo_li_try_push_back(fifo_head_li_t * head, fifo_node_li_t * new_node)
{
    assert(head && new_node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    FIFO_LI_NODE_INIT(new_node);
    int ret;
    if( (ret = FIFO_MTX_TRYLOCK(& head->mt)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR_BUSY or QUEUE_ERR */
        return ret;
    }

    fifo_li_push_back_no_lock(head, new_node);

    FIFO_MTX_UNLOCK(& head->mt);
    FIFO_CND_SIGNAL(& head->cond);
    return QUEUE_SUCCESS;
}

int fifo_li_wait_push_back(fifo_head_li_t *head, fifo_node_li_t *new_node)
{
    assert(head && new_node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    FIFO_LI_NODE_INIT(new_node);
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mt)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    fifo_li_push_back_no_lock(head, new_node);
    FIFO_MTX_UNLOCK(& head->mt);
    FIFO_CND_SIGNAL(& head->cond);
    return QUEUE_SUCCESS;
}

static inline fifo_node_li_t* fifo_li_pop_first_no_lock(fifo_head_li_t *head)
{
    if (FIFO_LI_IS_EMPTY(head))
        return NULL;

    fifo_node_li_t *node = head->begin;
    head->begin = node->next;
    if (head->begin == NULL)
        head->end = NULL;
    return node;
}

/* Infinite waiting until the event occurs or until the stop_flag is set */
int fifo_li_wait_pop_first(fifo_head_li_t *head, fifo_node_li_t **node)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mt)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    while(head->stop_flag || FIFO_LI_IS_EMPTY(head))
    {
        if(head->stop_flag || (ret != QUEUE_SUCCESS && ret != QUEUE_ERR_TIMEDOUT))
        {
            FIFO_MTX_UNLOCK(& head->mt);
            /* Returned QUEUE_STOP, QUEUE_ERR */
            return head->stop_flag ? QUEUE_STOP : ret;
        }
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, CONDITION_DELAY_NANO);
        ret = FIFO_CND_TIMEDWAIT(& head->cond, & head->mt, & ts);
    }

    *node = fifo_li_pop_first_no_lock(head);
    FIFO_MTX_UNLOCK(& head->mt);
    assert(*node != NULL);
    FIFO_LI_NODE_INIT((*node));
    return QUEUE_SUCCESS;
}

/* Waiting until the event occurs, until a timeout expires, or until the stop_flag is set */
int fifo_li_timedwait_pop_first(fifo_head_li_t *head, fifo_node_li_t **node, uint64_t timeout_ns)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mt)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    if(FIFO_LI_IS_EMPTY(head))
    {
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, timeout_ns);
        do {
            if(ret != QUEUE_SUCCESS || head->stop_flag)
            {
                FIFO_MTX_UNLOCK(& head->mt);
                /* Returned QUEUE_STOP, QUEUE_ERR_TIMEDOUT, QUEUE_ERR */
                return head->stop_flag ? QUEUE_STOP : ret;
            }
            ret = FIFO_CND_TIMEDWAIT(& head->cond, & head->mt, & ts);
        } while(FIFO_LI_IS_EMPTY(head));
    }

    *node = fifo_li_pop_first_no_lock(head);
    FIFO_MTX_UNLOCK(& head->mt);
    assert(*node == NULL);
    FIFO_LI_NODE_INIT((*node));
    return QUEUE_SUCCESS;
}

int fifo_li_try_pop_first(fifo_head_li_t * head, fifo_node_li_t **node)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    fifo_node_li_t *n;
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mt)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR or QUEUE_ERR_BUSY */
        return ret;
    }

    n = fifo_li_pop_first_no_lock(head);

    FIFO_MTX_UNLOCK(& head->mt);
    if (n != NULL)
    {
        FIFO_LI_NODE_INIT(n);
        *node = n;
        return QUEUE_SUCCESS;
    }
    return QUEUE_EMPTY;
}

int fifo_li_flush_step(fifo_head_li_t *head, fifo_node_li_t **node, int ret)
{
    assert(head && node);
    if(ret != QUEUE_SUCCESS && ret != QUEUE_AGAIN)
    {
        return ret;
    }

    fifo_node_li_t *n;
    if(ret == QUEUE_SUCCESS)
    {
        if( (ret = FIFO_MTX_LOCK(& head->mt)) != QUEUE_SUCCESS)
        {
            /* Returned QUEUE_ERR */
            return ret;
        }
    }

    n = fifo_li_pop_first_no_lock(head);
    if(n != NULL)
    {
        ret = n->next == NULL ? QUEUE_SUCCESS : QUEUE_AGAIN;
        FIFO_LI_NODE_INIT(n);
        *node = n;
    } else {
        ret = QUEUE_EMPTY;
    }

    if(ret != QUEUE_AGAIN)
        FIFO_MTX_UNLOCK(& head->mt);

    return ret;
}
