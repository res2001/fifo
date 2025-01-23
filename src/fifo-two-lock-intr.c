#include "fifo-two-lock-intr.h"
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>

#define FIFO_TLQI_IS_EMPTY(head) 						(head->begin == & head->fake_node && head->begin->next == NULL)
#define FIFO_TLQI_IS_NOT_EMPTY(head) 					(head->begin != & head->fake_node || head->begin->next != NULL)
#define FIFO_TLQI_NODE_INIT(node)						do { node->next = NULL; } while(0)

int fifo_tlqi_init(fifo_head_tlqi_t *head)
{
    assert( head);
    memset(head, 0, sizeof(*head));

    int stepcount = 0;
    int ret;
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

        head->begin = & head->fake_node;
        head->end = & head->fake_node;
        ++head->fake_in;
        return 0;
    } while(0);
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
        memset(head, 0, sizeof(*head));
        break;
    }
    return ret;
}

void fifo_tlqi_release_all(fifo_head_tlqi_t * head)
{
    head->stop_flag = 1;
    FIFO_CND_BROADCAST(& head->cond);
}

static inline void fifo_tlqi_push_back_no_lock(fifo_head_tlqi_t *head, fifo_node_tlqi_t *new_node)
{
    fifo_node_tlqi_t *prev = head->end;
    head->end = new_node;
    prev->next = new_node;
}

void fifo_tlqi_destroy(fifo_head_tlqi_t * head)
{
    fifo_tlqi_release_all(head);
    FIFO_CND_DESTROY(& head->cond);
    FIFO_MTX_DESTROY(& head->mend);
    FIFO_MTX_DESTROY(& head->mbegin);
    memset(head, 0, sizeof(*head));
}

int fifo_tlqi_try_push_back(fifo_head_tlqi_t * head, fifo_node_tlqi_t * new_node)
{
    assert(head && new_node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    FIFO_TLQI_NODE_INIT(new_node);
    int ret;
    if( (ret = FIFO_MTX_TRYLOCK(& head->mend)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR_BUSY or QUEUE_ERR */
        return ret;
    }

    fifo_tlqi_push_back_no_lock(head, new_node);

    FIFO_MTX_UNLOCK(& head->mend);
    FIFO_CND_SIGNAL(& head->cond);
    return QUEUE_SUCCESS;
}

int fifo_tlqi_wait_push_back(fifo_head_tlqi_t *head, fifo_node_tlqi_t *new_node)
{
    assert(head && new_node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    FIFO_TLQI_NODE_INIT(new_node);
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mend)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    fifo_tlqi_push_back_no_lock(head, new_node);
    FIFO_MTX_UNLOCK(& head->mend);
    FIFO_CND_SIGNAL(& head->cond);
    return QUEUE_SUCCESS;
}

static inline fifo_node_tlqi_t* fifo_tlqi_pop_first_no_lock(fifo_head_tlqi_t *head)
{
    fifo_node_tlqi_t *node, *next;
    node = head->begin;

    while(true)
    {
        next = node->next;
        if (next != NULL)
        {
            head->begin = next;
            if (node != & head->fake_node)
                return node;
            node = next;
            ++head->fake_out;
        } else if (node != & head->fake_node) {
            fifo_tlqi_wait_push_back(head, & head->fake_node);
            ++head->fake_in;
            head->begin = node->next;
            return node;
        } else
            return NULL;
    }
}

/* Infinite waiting until the event occurs or until the stop_flag is set */
int fifo_tlqi_wait_pop_first(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    while(head->stop_flag || FIFO_TLQI_IS_EMPTY(head))
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

    *node = fifo_tlqi_pop_first_no_lock(head);
    FIFO_MTX_UNLOCK(& head->mbegin);
    assert(*node != NULL);
    FIFO_TLQI_NODE_INIT((*node));
    return QUEUE_SUCCESS;
}

/* Waiting until the event occurs, until a timeout expires, or until the stop_flag is set */
int fifo_tlqi_timedwait_pop_first(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node, uint64_t timeout_ns)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    struct timespec ts;
    int ret;

    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR */
        return ret;
    }

    if(FIFO_TLQI_IS_EMPTY(head))
    {
        FIFO_CLOCK_GETTIME(& ts);
        FIFO_TIMESPEC_ADD_TIMEOUT(ts, timeout_ns);
        do {
            if(ret != QUEUE_SUCCESS || head->stop_flag)
            {
                FIFO_MTX_UNLOCK(& head->mbegin);
                /* Returned QUEUE_STOP, QUEUE_ERR_TIMEDOUT, QUEUE_ERR */
                return head->stop_flag ? QUEUE_STOP : ret;
            }
            ret = FIFO_CND_TIMEDWAIT(& head->cond, & head->mbegin, & ts);
        } while(FIFO_TLQI_IS_EMPTY(head));
    }

    *node = fifo_tlqi_pop_first_no_lock(head);
    FIFO_MTX_UNLOCK(& head->mbegin);
    assert(*node == NULL);
    FIFO_TLQI_NODE_INIT((*node));
    return QUEUE_SUCCESS;
}

int fifo_tlqi_try_pop_first(fifo_head_tlqi_t * head, fifo_node_tlqi_t **node)
{
    assert(head && node);
    if(head->stop_flag)
    {
        return QUEUE_STOP;
    }

    fifo_node_tlqi_t *n;
    int ret;
    if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
    {
        /* Returned QUEUE_ERR or QUEUE_ERR_BUSY */
        return ret;
    }

    n = fifo_tlqi_pop_first_no_lock(head);

    FIFO_MTX_UNLOCK(& head->mbegin);
    if (n != NULL)
    {
        FIFO_TLQI_NODE_INIT(n);
        *node = n;
        return QUEUE_SUCCESS;
    }
    return QUEUE_EMPTY;
}

int fifo_tlqi_flush_step(fifo_head_tlqi_t *head, fifo_node_tlqi_t **node, int ret)
{
    assert(head && node);
    if(ret != QUEUE_SUCCESS && ret != QUEUE_AGAIN)
    {
        return ret;
    }

    fifo_node_tlqi_t *n;
    if(ret == QUEUE_SUCCESS)
    {
        if( (ret = FIFO_MTX_LOCK(& head->mbegin)) != QUEUE_SUCCESS)
        {
            /* Returned QUEUE_ERR */
            return ret;
        }
    }

    n = fifo_tlqi_pop_first_no_lock(head);
    if(n != NULL)
    {
        ret = n->next == NULL ? QUEUE_SUCCESS : QUEUE_AGAIN;
        FIFO_TLQI_NODE_INIT(n);
        *node = n;
    } else {
        ret = QUEUE_EMPTY;
    }

    if(ret == QUEUE_SUCCESS || ret == QUEUE_EMPTY)
        FIFO_MTX_UNLOCK(& head->mbegin);

    return ret;
}
