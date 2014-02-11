#ifndef _SPINLOCK_XCHG_H
#define _SPINLOCK_XCHG_H

/* Spin lock using xchg.
 * Copied from http://locklessinc.com/articles/locks/
 */

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")

#define __HLE_ACQUIRE ".byte 0xf2 ; "
#define __HLE_RELEASE ".byte 0xf3 ; "

static inline unsigned short xchg_8(void *ptr, unsigned char x)
{
    __asm__ __volatile__(__HLE_ACQUIRE "xchgb %0,%1"
                :"=r" (x)
                :"m" (*(volatile unsigned char *)ptr), "0" (x)
                :"memory");

    return x;
}

#define BUSY 1
typedef unsigned char spinlock;

#define SPINLOCK_INITIALIZER 0

static inline void spin_lock(spinlock *lock)
{
    while (1) {
        if (!xchg_8(lock, BUSY)) return;
    
        while (*lock) cpu_relax();
    }
}

static inline void spin_unlock(spinlock *lock)
{
    __asm__ __volatile__(__HLE_RELEASE "movb $0, %0"
            :"=m" (*lock)
            :
            :"memory");
}

static inline int spin_trylock(spinlock *lock)
{
    return xchg_8(lock, BUSY);
}

#endif /* _SPINLOCK_XCHG_H */
