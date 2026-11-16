/* Threads and job pools: what the tools took from `concurrent.futures` (ThreadPoolExecutor and its map/submit/
 * as_completed, ProcessPoolExecutor and multiprocessing.Pool, whose work is CPU-bound and runs here on threads),
 * `threading` (Thread, Lock, RLock, Event, Semaphore), `os.cpu_count` and the -j fan-out of the compilers.
 *
 * Pool. Workers are threads started on demand (like ThreadPoolExecutor: one more thread per submit while fewer
 * than max_workers exist) and kept for the life of the pool; work is claimed from a queue, or, for map and
 * parallel-for, by an atomic index, so there is no per-item allocation and results land in input order. The
 * calling thread takes part in a map (so a map inside a task cannot starve the pool) and the number of tasks
 * running at once never exceeds the `nthreads` asked for. A task function returns an int (0 = success): that is
 * the replacement for an exception; the first failure in index order is reported as ThreadPoolExecutor.map raises
 * the first exception in order, and items that had not started when a failure was seen are skipped (cancelled).
 *
 * Primitives. AvpMutex is an SRW lock (non-recursive, exclusive or shared), AvpRMutex a CRITICAL_SECTION
 * (threading.RLock), AvpCond a CONDITION_VARIABLE, AvpOnce an INIT_ONCE, AvpEvent threading.Event, AvpSem
 * threading.Semaphore; AvpAtomic32/64 are sequentially consistent Interlocked operations. A zeroed AvpMutex,
 * AvpCond and AvpOnce is ready to use (static storage needs no init call). Timeouts are seconds as doubles;
 * negative means no limit, 0 only checks. */
#pragma once
#include "base.h"
#include "proc.h"

#include <intrin.h>

/* ---- cpu count ---- */
/* os.cpu_count(): the logical processors of all processor groups (GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)),
 * or PYTHON_CPU_COUNT from the environment when it is a number >= 1; never 0. */
int avp_cpu_count(void);
/* os.process_cpu_count(): on Windows the same value (CPython only subtracts an affinity mask on Linux). */
int avp_process_cpu_count(void);

/* ---- atomics ---- */
typedef struct { volatile long v; } AvpAtomic32;
typedef struct { volatile __int64 v; } AvpAtomic64;
static inline i32 avp_atomic_load32(AvpAtomic32 *a) { return (i32)_InterlockedOr(&a->v, 0); }
static inline void avp_atomic_store32(AvpAtomic32 *a, i32 x) { (void)_InterlockedExchange(&a->v, x); }
static inline i32 avp_atomic_add32(AvpAtomic32 *a, i32 d) { return (i32)_InterlockedExchangeAdd(&a->v, d) + d; } /* the new value */
static inline i32 avp_atomic_xchg32(AvpAtomic32 *a, i32 x) { return (i32)_InterlockedExchange(&a->v, x); }       /* the old value */
static inline bool avp_atomic_cas32(AvpAtomic32 *a, i32 expect, i32 desire) {
    return _InterlockedCompareExchange(&a->v, desire, expect) == expect;
}
static inline i64 avp_atomic_load64(AvpAtomic64 *a) { return (i64)_InterlockedOr64(&a->v, 0); }
static inline void avp_atomic_store64(AvpAtomic64 *a, i64 x) { (void)_InterlockedExchange64(&a->v, x); }
static inline i64 avp_atomic_add64(AvpAtomic64 *a, i64 d) { return (i64)_InterlockedExchangeAdd64(&a->v, d) + d; }
static inline i64 avp_atomic_xchg64(AvpAtomic64 *a, i64 x) { return (i64)_InterlockedExchange64(&a->v, x); }
static inline bool avp_atomic_cas64(AvpAtomic64 *a, i64 expect, i64 desire) {
    return _InterlockedCompareExchange64(&a->v, desire, expect) == expect;
}
/* Raises *max to x when x is larger (a high-water mark); returns the maximum now held. */
i64 avp_atomic_max64(AvpAtomic64 *a, i64 x);

/* ---- mutex, condition variable, once ---- */
typedef struct { void *p; } AvpMutex; /* SRWLOCK */
void avp_mutex_lock(AvpMutex *m);
bool avp_mutex_trylock(AvpMutex *m);
void avp_mutex_unlock(AvpMutex *m);
void avp_mutex_lock_shared(AvpMutex *m); /* reader side of the same lock */
bool avp_mutex_trylock_shared(AvpMutex *m);
void avp_mutex_unlock_shared(AvpMutex *m);

typedef struct { void *p[5]; } AvpRMutex; /* CRITICAL_SECTION (40 bytes on x64) */
void avp_rmutex_init(AvpRMutex *m);
void avp_rmutex_destroy(AvpRMutex *m);
void avp_rmutex_lock(AvpRMutex *m);
bool avp_rmutex_trylock(AvpRMutex *m);
void avp_rmutex_unlock(AvpRMutex *m);

typedef struct { void *p; } AvpCond; /* CONDITION_VARIABLE */
/* Releases m (held exclusively), waits for a signal or the timeout, takes m again. False on timeout. Spurious
 * wake-ups happen: wait in a loop on the condition. */
bool avp_cond_wait(AvpCond *c, AvpMutex *m, double timeout);
void avp_cond_signal(AvpCond *c);
void avp_cond_broadcast(AvpCond *c);

typedef struct { void *p; } AvpOnce; /* INIT_ONCE */
/* Runs fn(ctx) once per AvpOnce, whichever thread gets there first; the others wait for it. */
void avp_once(AvpOnce *o, void (*fn)(void *ctx), void *ctx);

typedef struct { void *h; } AvpEvent; /* threading.Event: a manual-reset event */
void avp_event_init(AvpEvent *e);
void avp_event_destroy(AvpEvent *e);
void avp_event_set(AvpEvent *e);
void avp_event_clear(AvpEvent *e);
bool avp_event_is_set(AvpEvent *e);
bool avp_event_wait(AvpEvent *e, double timeout); /* Event.wait(timeout): true when set */

typedef struct { void *h; } AvpSem; /* threading.Semaphore */
void avp_sem_init(AvpSem *s, int value);
void avp_sem_destroy(AvpSem *s);
bool avp_sem_acquire(AvpSem *s, double timeout);
void avp_sem_release(AvpSem *s);

/* ---- threads ---- */
typedef struct AvpThread AvpThread;
/* threading.Thread(target=fn, args=(arg,)).start(); stack_bytes 0: the default (threading.stack_size()). */
AvpThread *avp_thread_start(void (*fn)(void *arg), void *arg, size_t stack_bytes);
void avp_thread_join(AvpThread *t); /* Thread.join(); frees the handle */
void avp_thread_detach(AvpThread *t); /* a daemon thread: no join, ends with the process */
u32 avp_thread_id(void);              /* threading.get_ident() */

/* ---- pools ---- */
typedef struct AvpPool AvpPool;
typedef struct {
    int threads;         /* max_workers (<= 0: avp_cpu_count()) */
    size_t stack_bytes;  /* per worker thread; 0: default */
    /* ProcessPoolExecutor(initializer=...): runs on each worker thread before its first task, with its index. */
    void (*thread_init)(int worker_index, void *ctx);
    void *ctx;
} AvpPoolOpts;
AvpPool *avp_pool_new(int threads);
AvpPool *avp_pool_new_ex(const AvpPoolOpts *opts);
/* Executor.shutdown(wait=True) and free: tasks still queued are cancelled when cancel_pending, else run first. */
void avp_pool_shutdown(AvpPool *p, bool cancel_pending);
/* 0..threads-1 on a pool worker thread, -1 elsewhere, which includes the thread that called a map and works in it
 * (per-thread state such as a capstone handle: keep it in AVP_THREAD_LOCAL storage, or index with this and treat -1
 * as one more slot). */
int avp_pool_worker_index(void);

/* Executor.map(fn, items): runs fn for every index, results in place. */
typedef int (*AvpMapFn)(void *ctx, size_t index, const void *item, void *result);
/* On `pool` (NULL: the process-wide default pool, grown to nthreads). items and results may be NULL (index-only
 * work); item i is items + i * item_size, its result slot results + i * result_size. nthreads <= 0 uses
 * avp_cpu_count(), 1 runs on the calling thread alone. Returns -1 when every item returned 0, else the lowest
 * index whose fn returned non-zero (later unstarted items are skipped and their result slots left as they were). */
i64 avp_pool_map(AvpPool *pool, int nthreads, AvpMapFn fn, void *ctx, const void *items, size_t count,
                 size_t item_size, void *results, size_t result_size);
/* The same for a plain function of the index. */
typedef int (*AvpForFn)(void *ctx, size_t index);
i64 avp_pool_for(AvpPool *pool, int nthreads, AvpForFn fn, void *ctx, size_t count);
/* Parallel-for over [0, count) in chunks of `grain` indices (CPU-bound loops): fn(ctx, begin, end) per chunk. */
typedef void (*AvpRangeFn)(void *ctx, size_t begin, size_t end);
void avp_pool_for_range(AvpPool *pool, int nthreads, AvpRangeFn fn, void *ctx, size_t count, size_t grain);

/* Executor.submit: the future of one task. */
typedef struct AvpFuture AvpFuture;
typedef struct AvpCompletions AvpCompletions;
AvpFuture *avp_pool_submit(AvpPool *p, int (*fn)(void *arg), void *arg);
/* As submit, and the future is also queued on `done` when it finishes (concurrent.futures.as_completed). */
AvpFuture *avp_pool_submit_to(AvpPool *p, int (*fn)(void *arg), void *arg, AvpCompletions *done);
bool avp_future_done(AvpFuture *f);
/* Future.result(timeout): waits; true with *result = fn's return when finished, false on timeout or when the task
 * was cancelled. */
bool avp_future_result(AvpFuture *f, double timeout, int *result);
bool avp_future_cancel(AvpFuture *f); /* true when it had not started */
bool avp_future_cancelled(AvpFuture *f);
/* Waits for a running task, then frees. Allowed after avp_pool_shutdown (every future is then final). */
void avp_future_free(AvpFuture *f);

AvpCompletions *avp_completions_new(void);
/* The next finished future in completion order, NULL on timeout (the caller counts what it submitted). */
AvpFuture *avp_completions_next(AvpCompletions *c, double timeout);
void avp_completions_free(AvpCompletions *c);

/* ---- the compiler fan-out: up to max_parallel processes at once ---- */
typedef void (*AvpProcDoneFn)(void *ctx, size_t index, AvpProcResult *r);
/* Runs specs[0..n) with at most max_parallel running at once (<= 0: avp_cpu_count()), each from a pool worker
 * thread that starts it, captures its output and calls done(ctx, index, &result) on that thread when it ends (the
 * callback may run on several threads at once; serialise inside it if needed). Results go to results[index] when
 * results is not NULL (their text lives in the worker thread's arena, valid to the end of the process). Starting
 * order is the index order. Returns the count of processes that started and ended without AVP_PROC_START_FAILED. */
size_t avp_procpool_run(int max_parallel, const AvpProcSpec *specs, size_t n, AvpProcDoneFn done, void *ctx,
                        AvpProcResult *results);
