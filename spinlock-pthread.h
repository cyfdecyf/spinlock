#ifndef _SPINLOCK_PTHREAD_H
#define _SPINLOCK_PTHREAD_H

#define SPINLOCK_ATTR static __inline __attribute__((always_inline, no_instrument_function))

#define spinlock pthread_mutex_t

SPINLOCK_ATTR void spin_lock(spinlock *lock)
{
    pthread_mutex_lock(lock);
}

SPINLOCK_ATTR void spin_unlock(spinlock *lock)
{
    pthread_mutex_unlock(lock);
}

#define SPINLOCK_INITIALIZER { 0 }

#endif /* _SPINLOCK_H */
