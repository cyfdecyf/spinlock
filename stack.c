#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include "spinlock-pthread.h"

/*
 * Naive implementation of lock-free stack which does not handle ABA problem.
 * This works if only one thread is doing pop.
 *
 * For lock-free stack which handles ABA problem, see streamflow.
 */

/* Return 1 if swap happened. */
static inline unsigned int compare_and_swap(volatile void *address,
        void *old_value, void *new_value)
{
    void *prev = 0;

    asm volatile("lock; cmpxchgq %1,%2"
        : "=a"(prev)
        : "r"(new_value), "m"(*(long *)address), "0"(old_value)
        : "memory");

    return prev == old_value;
}

/* Flag to use pthread mutex or spinlock, useful when we want to compare the
 * performance of different implementation. */
/*#define MUTEX*/
/*#define SPINLOCK*/

typedef struct Node {
    struct Node *next;
    int val;
} Node;

typedef struct {
    volatile Node *top;
#ifdef MUTEX
    pthread_mutex_t mutex;
#elif defined(SPINLOCK)
    Spinlock slock;
#endif
} Stack;

Stack gstack;

#if defined(MUTEX) || defined(SPINLOCK)

void push(Stack *stack, Node *n) {
#ifdef MUTEX
    pthread_mutex_lock(&stack->mutex);
#elif defined(SPINLOCK)
    spin_lock(&stack->slock);
#endif

    n->next = (Node *)stack->top;
    stack->top = n;

#ifdef MUTEX
    pthread_mutex_unlock(&stack->mutex);
#elif defined(SPINLOCK)
    spin_unlock(&stack->slock);
#endif
}

Node *pop(Stack *stack) {
    Node *oldtop;

    if (stack->top == NULL)
        return NULL;

#ifdef MUTEX
    pthread_mutex_lock(&stack->mutex);
#elif defined(SPINLOCK)
    spin_lock(&stack->slock);
#endif
    oldtop = (Node *)stack->top;
    stack->top = oldtop->next;

#ifdef MUTEX
    pthread_mutex_unlock(&stack->mutex);
#elif defined(SPINLOCK)
    spin_unlock(&stack->slock);
#endif

    return oldtop;
}

#else

/* Lock free version. */
void push(Stack *stack, Node *n) {
    Node *oldtop;
    while (1) {
        oldtop = (Node *)stack->top;
        n->next = oldtop;
        if (compare_and_swap(&stack->top, oldtop, n))
            return;
    }
}

Node *pop(Stack *stack) {
    Node *oldtop, *next;

    while (1) {
        oldtop = (Node *)stack->top;
        if (oldtop == NULL)
            return NULL;
        next = oldtop->next;
        if (compare_and_swap(&stack->top, oldtop, next))
            return oldtop;
    }
}

#endif

/* Testing code. */

#define NITERS 2000000 /* Number of pushes for each push thread. */
#define NTHR 3 /* Number of push threads. */

void *pusher(void *dummy) {
    long i, tid = (long) dummy;
    for (i = 0; i < NITERS; i++) {
        Node *n = malloc(sizeof(*n));
        n->val = NTHR * i + tid;
        push(&gstack, n);
    }

    return NULL;
}

static inline void atomic_inc64(volatile unsigned long* address)
{
    asm volatile(
        "lock; incq %0\n\t"
        : "=m" (*address)
        : "m" (*address));
}

volatile unsigned long popcount = 0;

void *poper(void *dummy) {
    Node *n;

    while (popcount < NTHR * NITERS) {
        n = pop(&gstack);
        if (n) {
            printf("%d\n", n->val);
            /* Only one pop thread. */
            popcount++;
            /*atomic_inc64(&popcount);*/
        }
    }

    return NULL;
}

int main(int argc, const char *argv[]) {
#ifdef MUTEX
    pthread_mutex_init(&gstack.mutex, NULL);
#endif

#ifdef SPINLOCK
    SPIN_LOCK_INIT(&gstack.slock);
#endif

    /* TODO We need more threads to test the scalability of the stack
     * implementation.
     * 
     * With 3 pusher calling push,
     * 1. spinlock give worst performance.
     * 2. mutex is faster than spinlock, but it's running time is not stable.
     * 3. lock free version is fast, and running time is stable.
     */

    pthread_t thr_push[NTHR], thr_pop;
    long i;

    for (i = 0; i < NTHR; i++) {
        if (pthread_create(&thr_push[i], NULL, pusher, (void *)i) != 0) {
            perror("thread creating failed");
        }
    }

    if (pthread_create(&thr_pop, NULL, poper, NULL) != 0) {
        perror("thread creating failed");
    }

    for (i = 0; i < NTHR; i++) {
        pthread_join(thr_push[i], NULL);
    }
    pthread_join(thr_pop, NULL);

    return 0;
}

