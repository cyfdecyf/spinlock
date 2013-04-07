#ifndef _SPINLOCK_CMPXCHG_H
#define _SPINLOCK_CMPXCHG_H

typedef struct {
    volatile char lock;
} spinlock;

#define SPINLOCK_ATTR static __inline __attribute__((always_inline, no_instrument_function))

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

SPINLOCK_ATTR char __testandset(spinlock *p)
{
    char readval = 0;

    asm volatile (
            "lock; cmpxchgb %b2, %0"
            : "+m" (p->lock), "+a" (readval)
            : "r" (1)
            : "cc");
    return readval;
}

SPINLOCK_ATTR void spin_lock(spinlock *lock)
{
    while (__testandset(lock)) {
        cpu_relax();
    }
}

SPINLOCK_ATTR void spin_unlock(spinlock *s)
{
    s->lock = 0;
}

#define SPINLOCK_INITIALIZER { 0 }

#endif /* _SPINLOCK_CMPXCHG_H */
