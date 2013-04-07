#ifndef _SPINLOCK_MCS
#define _SPINLOCK_MCS

#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))

#define barrier() asm volatile("": : :"memory")
#define cpu_relax() asm volatile("pause\n": : :"memory")

static inline void *xchg_64(void *ptr, void *x)
{
    __asm__ __volatile__("xchgq %0,%1"
                :"=r" ((unsigned long long) x)
                :"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
                :"memory");

    return x;
}

typedef struct mcs_lock_t mcs_lock_t;
struct mcs_lock_t
{
    mcs_lock_t *next;
    int spin;
};
typedef struct mcs_lock_t *mcs_lock;

static inline void lock_mcs(mcs_lock *m, mcs_lock_t *me)
{
    mcs_lock_t *tail;
    
    me->next = NULL;
    me->spin = 0;

    tail = xchg_64(m, me);
    
    /* No one there? */
    if (!tail) return;

    /* Someone there, need to link in */
    tail->next = me;

    /* Make sure we do the above setting of next. */
    barrier();
    
    /* Spin on my spin variable */
    while (!me->spin) cpu_relax();
    
    return;
}

static inline void unlock_mcs(mcs_lock *m, mcs_lock_t *me)
{
    /* No successor yet? */
    if (!me->next)
    {
        /* Try to atomically unlock */
        if (cmpxchg(m, me, NULL) == me) return;
    
        /* Wait for successor to appear */
        while (!me->next) cpu_relax();
    }

    /* Unlock next one */
    me->next->spin = 1; 
}

static inline int trylock_mcs(mcs_lock *m, mcs_lock_t *me)
{
    mcs_lock_t *tail;
    
    me->next = NULL;
    me->spin = 0;
    
    /* Try to lock */
    tail = cmpxchg(m, NULL, &me);
    
    /* No one was there - can quickly return */
    if (!tail) return 0;
    
    return 1; // Busy
}

#endif
