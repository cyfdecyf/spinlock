#ifndef _SPINLOCK_K42
#define _SPINLOCK_K42

/* Code copied from http://locklessinc.com/articles/locks/
 * Note this algorithm is patented by IBM. */

/* Macro hack to avoid changing the name. */
#define spin_lock k42_lock
#define spin_unlock k42_unlock
#define spinlock k42lock

#define SPINLOCK_INITIALIZER { 0, 0 };

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

typedef struct k42lock k42lock;
struct k42lock
{
    k42lock *next;
    k42lock *tail;
};

static inline void k42_lock(k42lock *l)
{
    k42lock me;
    k42lock *pred, *succ;
    me.next = NULL;
    
    barrier();
    
    pred = xchg_64(&l->tail, &me);
    if (pred)
    {
        me.tail = (void *) 1;
        
        barrier();
        pred->next = &me;
        barrier();
        
        while (me.tail) cpu_relax();
    }
    
    succ = me.next;

    if (!succ)
    {
        barrier();
        l->next = NULL;
        
        if (cmpxchg(&l->tail, &me, &l->next) != &me)
        {
            while (!me.next) cpu_relax();
            
            l->next = me.next;
        }
    }
    else
    {
        l->next = succ;
    }
}

static inline void k42_unlock(k42lock *l)
{
    k42lock *succ = l->next;
    
    barrier();
    
    if (!succ)
    {
        if (cmpxchg(&l->tail, &l->next, NULL) == (void *) &l->next) return;
        
        while (!l->next) cpu_relax();
        succ = l->next;
    }
    
    succ->tail = NULL;
}

static inline int k42_trylock(k42lock *l)
{
    if (!cmpxchg(&l->tail, NULL, &l->next)) return 0;
    
    return 1; // Busy
}

#endif
