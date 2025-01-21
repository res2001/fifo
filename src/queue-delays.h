#ifndef QUEUE_DELAYS_H_
#define QUEUE_DELAYS_H_

#define TRY_AGAIN_COUNT                 3
// Condition variable delay in nanoseconds (1 ms)
#define CONDITION_DELAY_NANO            1000000
// The maximum number of waits on a condition variable (1 sec)
#define MAX_WAIT_FREE_NODE              1000000000 / CONDITION_DELAY_NANO

#endif /* QUEUE_DELAYS_H_ */
