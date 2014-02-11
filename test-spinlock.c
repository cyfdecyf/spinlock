#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <errno.h>

#ifdef XCHG
#include "spinlock-xchg.h"
#elif defined(XCHGBACKOFF)
#include "spinlock-xchg-backoff.h"
#elif defined(K42)
#include "spinlock-k42.h"
#elif defined(MCS)
#include "spinlock-mcs.h"
#elif defined(TICKET)
#include "spinlock-ticket.h"
#elif defined(PTHREAD)
#include "spinlock-pthread.h"
#elif defined(CMPXCHG)
#include "spinlock-cmpxchg.h"
#elif defined(RTM)
#include "spinlock-xchg.h"
#include "rtm.h"
#elif defined(HLE)
#include "spinlock-xchg-hle.h"
#else
#error "must define a spinlock implementation"
#endif

#ifndef cpu_relax
#define cpu_relax() asm volatile("pause\n": : :"memory")
#endif

/* It's hard to say which spinlock implementation performs best. I guess the
 * performance depends on CPU topology which will affect the cache coherence
 * messages, and maybe other factors.
 *
 * The result of binding threads to the same physical core shows that when
 * threads are on a single physical CPU, contention will not cause severe
 * performance degradation. But there's one exception, cmpxchg. It's performance
 * will degrade no matter the threads resides on the which physical CPU.
 *
 * Here's the result on Dell R910 server (4 CPUs, 10 cores each), with Intel(R)
 * Xeon(R) CPU E7- 4850  @ 2.00GHz
 *
 * - pthread_mutex BEATS ALL when there's more than 2 cores running this
 *   benchmark! Not sure what's the trick used by pthread_mutex.
 *
 * - For spinlock with cmpxchg, the performance degrades very fast with the
 * increase of threads. Seems that it does not have good scalability.
 *
 * - For spinlock with xchg, it has much better scalability than cmpxchg, but it's
 * slower when there's 2 and 4 cores.
 *
 * - For K42, The case for 2 threads is extremely bad, for other number of
 *   threads, the performance is stable and shows good scalability.
 *
 * - MCS spinlock has similar performance with K42, and does not have the
 *   extremely bad 2 thread case.
 *
 * - Ticket spinlock actually performs very badly.
 */

/* Number of total lock/unlock pair.
 * Note we need to ensure the total pair of lock and unlock opeartion are the
 * same no matter how many threads are used. */
#define N_PAIR 16000000

/* Bind threads to specific cores. The goal is to make threads locate on the
 * same physical CPU. Modify bind_core before using this. */
//#define BIND_CORE

static int nthr = 0;

static volatile uint32_t wflag;
/* Wait on a flag to make all threads start almost at the same time. */
void wait_flag(volatile uint32_t *flag, uint32_t expect) {
    __sync_fetch_and_add((uint32_t *)flag, 1);
    while (*flag != expect) {
        cpu_relax();
    }
}

static struct timeval start_time;
static struct timeval end_time;

static void calc_time(struct timeval *start, struct timeval *end) {
    if (end->tv_usec < start->tv_usec) {
        end->tv_sec -= 1;
        end->tv_usec += 1000000;
    }

    assert(end->tv_sec >= start->tv_sec);
    assert(end->tv_usec >= start->tv_usec);
    struct timeval interval = {
        end->tv_sec - start->tv_sec,
        end->tv_usec - start->tv_usec
    };
    printf("%ld.%06ld\t", (long)interval.tv_sec, (long)interval.tv_usec);
}

// Use an array of counter to see effect on RTM if touches more memory.
#define NCOUNTER 1

// Use thread local counter to avoid cache contention between cores.
// For TSX, this avoids TX conflicts so the performance overhead/improvement is
// due to TSX mechanism.
static __thread int counter[NCOUNTER];

#ifdef MCS
mcs_lock cnt_lock = NULL;
#else
spinlock sl;
#endif

#ifdef BIND_CORE
void bind_core(int threadid) {
    /* cores with logical id 4x   is on CPU physical id 0 */
    /* cores with logical id 4x+1 is on CPU physical id 1 */
    int phys_id = threadid / 10;
    int core = threadid % 10;

    int logical_id = 4 * core + phys_id;
    /*printf("thread %d bind to logical core %d on physical id %d\n", threadid, logical_id, phys_id);*/

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(logical_id, &set);

    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("Set affinity failed");
        exit(EXIT_FAILURE);
    }
}
#endif

void *inc_thread(void *id) {
    int n = N_PAIR / nthr;
    assert(n * nthr == N_PAIR);
#ifdef MCS
    mcs_lock_t local_lock;
#endif
#ifdef BIND_CORE
    bind_core((int)(long)(id));
#endif
    wait_flag(&wflag, nthr);

    if (((long) id == 0)) {
        /*printf("get start time\n");*/
        gettimeofday(&start_time, NULL);
    }

    /* Start lock unlock test. */
    for (int i = 0; i < n; i++) {
#ifdef MCS
        lock_mcs(&cnt_lock, &local_lock);
        for (int j = 0; j < NCOUNTER; j++) counter[j]++;
        unlock_mcs(&cnt_lock, &local_lock);
#elif RTM
        int status;
        if ((status = _xbegin()) == _XBEGIN_STARTED) {
            for (int j = 0; j < NCOUNTER; j++) counter[j]++;
            if (sl == BUSY)
                _xabort(1);
            _xend();
        } else {
            spin_lock(&sl);
            for (int j = 0; j < NCOUNTER; j++) counter[j]++;
            spin_unlock(&sl);
        }
#else
        spin_lock(&sl);
        for (int j = 0; j < NCOUNTER; j++) counter[j]++;
        spin_unlock(&sl);
#endif
    }

    if (__sync_fetch_and_add((uint32_t *)&wflag, -1) == 1) {
        /*printf("get end time\n");*/
        gettimeofday(&end_time, NULL);
    }
    return NULL;
}

int main(int argc, const char *argv[])
{
    pthread_t *thr;
    int ret = 0;

    if (argc != 2) {
        printf("Usage: %s <num of threads>\n", argv[0]);
        exit(1);
    }

    nthr = atoi(argv[1]);
    /*printf("using %d threads\n", nthr);*/
    thr = calloc(sizeof(*thr), nthr);

    // Start thread
    for (long i = 0; i < nthr; i++) {
        if (pthread_create(&thr[i], NULL, inc_thread, (void *)i) != 0) {
            perror("thread creating failed");
        }
    }
    // join thread
    for (long i = 0; i < nthr; i++)
        pthread_join(thr[i], NULL);

    calc_time(&start_time, &end_time);
    /*
     *for (int i = 0; i < NCOUNTER; i++) {
     *    if (counter[i] == N_PAIR) {
     *    } else {
     *        printf("counter %d error\n", i);
     *        ret = 1;
     *    }
     *}
     */

    return ret;
}
