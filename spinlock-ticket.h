#ifndef _SPINLOCK_TICKET_H
#define _SPINLOCK_H

/* Code copied from http://locklessinc.com/articles/locks/ */

#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))

#define barrier() asm volatile("": : :"memory")
#define cpu_relax() asm volatile("pause\n": : :"memory")

#define spin_lock ticket_lock
#define spin_unlock ticket_unlock
#define spinlock ticketlock

#define SPINLOCK_INITIALIZER { 0, 0 };

typedef union ticketlock ticketlock;

union ticketlock
{
    unsigned u;
    struct
    {
        unsigned short ticket;
        unsigned short users;
    } s;
};

static inline void ticket_lock(ticketlock *t)
{
    unsigned short me = atomic_xadd(&t->s.users, 1);
    
    while (t->s.ticket != me) cpu_relax();
}

static inline void ticket_unlock(ticketlock *t)
{
    barrier();
    t->s.ticket++;
}

static inline int ticket_trylock(ticketlock *t)
{
    unsigned short me = t->s.users;
    unsigned short menew = me + 1;
    unsigned cmp = ((unsigned) me << 16) + me;
    unsigned cmpnew = ((unsigned) menew << 16) + me;

    if (cmpxchg(&t->u, cmp, cmpnew) == cmp) return 0;
    
    return 1; // Busy
}

static inline int ticket_lockable(ticketlock *t)
{
    ticketlock u = *t;
    barrier();
    return (u.s.ticket == u.s.users);
}

#endif
