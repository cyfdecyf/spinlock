Various spinlock implementations from this article [Spinlocks and Read-Write Locks](http://locklessinc.com/articles/locks/) by *Lockless Inc*.

I made some modification to make each implementation self contained and provide a benchmark script. The code relies on GCC's built-in functions for atomic memory access.

**Note: Scalability is achieved by avoiding sharing and contention, not by scalable locks.**
