Implementation of the "Two-Lock Concurrent Queue Algorithm" described in the "Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms",
Maged M. Michael, Michael L. Scott, PODC96
https://www.cs.rochester.edu/research/synchronization/pseudocode/queues.html

1. fifo-two-lock-fix - Two-lock FIFO Queue implementation with built-in fixed node allocator.
2. fifo-two-lock-intr - Two-lock FIFO Queue implementation with intrusive nodes.
3. fifo-lock-free-intr - Implementation Lock-Free FIFO Queue with many enqueuer and one dequeuer with intrusive nodes.
4. fifo-lock-intr - simple intrusive queue with one lock based on a single-link list.
5. queue-fix-allocator - Allocator of blocks of the same length based on fifo-two-lock-fix
