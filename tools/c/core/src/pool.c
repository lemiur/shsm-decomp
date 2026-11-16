/* Threads, synchronisation primitives and job pools (pool.h). */
#include "avp/pool.h"
#include "avp/arena.h"
#include "avp/clock.h"

#include <math.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>

/* ---- cpu count ---- */

int avp_cpu_count(void) {
    /* CPython: PYTHON_CPU_COUNT (a number >= 1) overrides, else GetActiveProcessorCount(ALL_PROCESSOR_GROUPS). */
    static AvpAtomic32 cached; /* 0: not computed */
    i32 c = avp_atomic_load32(&cached);
    if (c) return c;
    int n = 0;
    const char *env = getenv("PYTHON_CPU_COUNT");
    if (env && *env) {
        char *end;
        long v = strtol(env, &end, 10);
        if (*end == '\0' && v >= 1 && v < 1000000) n = (int)v;
    }
    if (n == 0) n = (int)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (n < 1) n = 1;
    avp_atomic_store32(&cached, n);
    return n;
}

int avp_process_cpu_count(void) { return avp_cpu_count(); }

/* ---- atomics ---- */

i64 avp_atomic_max64(AvpAtomic64 *a, i64 x) {
    for (;;) {
        i64 cur = avp_atomic_load64(a);
        if (cur >= x) return cur;
        if (avp_atomic_cas64(a, cur, x)) return x;
    }
}

/* ---- mutex, condition variable, once, event, semaphore ---- */

static DWORD timeout_ms(double timeout) {
    if (timeout < 0) return INFINITE;
    double ms = ceil(timeout * 1000.0);
    return ms >= (double)(INFINITE - 1) ? INFINITE - 1 : (DWORD)ms;
}

void avp_mutex_lock(AvpMutex *m) { AcquireSRWLockExclusive((PSRWLOCK)m); }
bool avp_mutex_trylock(AvpMutex *m) { return TryAcquireSRWLockExclusive((PSRWLOCK)m) != 0; }
void avp_mutex_unlock(AvpMutex *m) { ReleaseSRWLockExclusive((PSRWLOCK)m); }
void avp_mutex_lock_shared(AvpMutex *m) { AcquireSRWLockShared((PSRWLOCK)m); }
bool avp_mutex_trylock_shared(AvpMutex *m) { return TryAcquireSRWLockShared((PSRWLOCK)m) != 0; }
void avp_mutex_unlock_shared(AvpMutex *m) { ReleaseSRWLockShared((PSRWLOCK)m); }

void avp_rmutex_init(AvpRMutex *m) {
    AVP_ASSERT(sizeof(AvpRMutex) >= sizeof(CRITICAL_SECTION));
    InitializeCriticalSectionAndSpinCount((LPCRITICAL_SECTION)m, 2000);
}
void avp_rmutex_destroy(AvpRMutex *m) { DeleteCriticalSection((LPCRITICAL_SECTION)m); }
void avp_rmutex_lock(AvpRMutex *m) { EnterCriticalSection((LPCRITICAL_SECTION)m); }
bool avp_rmutex_trylock(AvpRMutex *m) { return TryEnterCriticalSection((LPCRITICAL_SECTION)m) != 0; }
void avp_rmutex_unlock(AvpRMutex *m) { LeaveCriticalSection((LPCRITICAL_SECTION)m); }

bool avp_cond_wait(AvpCond *c, AvpMutex *m, double timeout) {
    return SleepConditionVariableSRW((PCONDITION_VARIABLE)c, (PSRWLOCK)m, timeout_ms(timeout), 0) != 0;
}
void avp_cond_signal(AvpCond *c) { WakeConditionVariable((PCONDITION_VARIABLE)c); }
void avp_cond_broadcast(AvpCond *c) { WakeAllConditionVariable((PCONDITION_VARIABLE)c); }

typedef struct {
    void (*fn)(void *);
    void *ctx;
} OnceCall;
static BOOL CALLBACK once_trampoline(PINIT_ONCE once, PVOID param, PVOID *ctx) {
    AVP_UNUSED(once);
    AVP_UNUSED(ctx);
    OnceCall *c = param;
    c->fn(c->ctx);
    return TRUE;
}
void avp_once(AvpOnce *o, void (*fn)(void *ctx), void *ctx) {
    OnceCall c = {fn, ctx};
    InitOnceExecuteOnce((PINIT_ONCE)o, once_trampoline, &c, NULL);
}

void avp_event_init(AvpEvent *e) {
    e->h = CreateEventW(NULL, TRUE, FALSE, NULL);
    AVP_ASSERT(e->h != NULL);
}
void avp_event_destroy(AvpEvent *e) {
    if (e->h) CloseHandle(e->h);
    e->h = NULL;
}
void avp_event_set(AvpEvent *e) { SetEvent(e->h); }
void avp_event_clear(AvpEvent *e) { ResetEvent(e->h); }
bool avp_event_is_set(AvpEvent *e) { return WaitForSingleObject(e->h, 0) == WAIT_OBJECT_0; }
bool avp_event_wait(AvpEvent *e, double timeout) {
    return WaitForSingleObject(e->h, timeout_ms(timeout)) == WAIT_OBJECT_0;
}

void avp_sem_init(AvpSem *s, int value) {
    s->h = CreateSemaphoreW(NULL, value, 0x7fffffff, NULL);
    AVP_ASSERT(s->h != NULL);
}
void avp_sem_destroy(AvpSem *s) {
    if (s->h) CloseHandle(s->h);
    s->h = NULL;
}
bool avp_sem_acquire(AvpSem *s, double timeout) {
    return WaitForSingleObject(s->h, timeout_ms(timeout)) == WAIT_OBJECT_0;
}
void avp_sem_release(AvpSem *s) { ReleaseSemaphore(s->h, 1, NULL); }

/* ---- threads ---- */

struct AvpThread {
    HANDLE h;
    void (*fn)(void *);
    void *arg;
};

static unsigned __stdcall thread_entry(void *p) {
    AvpThread *t = p;
    t->fn(t->arg);
    return 0;
}

AvpThread *avp_thread_start(void (*fn)(void *arg), void *arg, size_t stack_bytes) {
    AvpThread *t = avp_xcalloc(1, sizeof *t);
    t->fn = fn;
    t->arg = arg;
    t->h = (HANDLE)_beginthreadex(NULL, (unsigned)stack_bytes, thread_entry, t, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!t->h) avp_fatal("cannot start a thread (error %lu)", (unsigned long)GetLastError());
    return t;
}

void avp_thread_join(AvpThread *t) {
    WaitForSingleObject(t->h, INFINITE);
    CloseHandle(t->h);
    avp_xfree(t);
}

void avp_thread_detach(AvpThread *t) {
    /* The entry point reads fn and arg before it can be freed: wait for nothing, the struct is leaked (24 bytes). */
    CloseHandle(t->h);
}

u32 avp_thread_id(void) { return GetCurrentThreadId(); }

/* ---- the pool ---- */

typedef struct Task Task;
typedef struct MapJob MapJob;

struct Task {
    Task *next, *prev;
    void (*run)(Task *self);
    MapJob *map; /* a helper of a map: counted in map->running while it executes */
    AvpFuture *future;
};

struct MapJob {
    enum { JOB_MAP, JOB_FOR, JOB_RANGE } kind;
    AvpMapFn mapfn;
    AvpForFn forfn;
    AvpRangeFn rangefn;
    void *ctx;
    const u8 *items;
    u8 *results;
    size_t count, item_size, result_size, grain;
    AvpAtomic64 next;
    AvpAtomic64 fail; /* lowest failing index + 1, 0 when none */
    int running;      /* helpers executing (under pool->mu) */
    AvpPool *pool;
};

struct AvpPool {
    AvpMutex mu;
    AvpCond work_cv; /* workers wait here */
    AvpCond done_cv; /* futures, maps and shutdown wait here */
    Task *head, *tail;
    size_t queued;
    int idle;
    int fresh; /* started, not yet in the queue loop */
    int max_threads;
    int nstarted;
    bool shutdown;
    AvpPoolOpts opts;
    AvpThread **threads;
    size_t threads_cap;
};

static AVP_THREAD_LOCAL int t_worker_index = -1;

int avp_pool_worker_index(void) { return t_worker_index; }

struct AvpFuture {
    AvpAtomic32 state; /* 0 pending, 1 running, 2 done, 3 cancelled */
    AvpPool *pool;
    int (*fn)(void *);
    void *arg;
    int result;
    Task task;
    AvpCompletions *done_q;
    AvpFuture *next_done;
};

struct AvpCompletions {
    AvpMutex mu;
    AvpCond cv;
    AvpFuture *head, *tail;
};

static void queue_push(AvpPool *p, Task *t) {
    t->next = NULL;
    t->prev = p->tail;
    if (p->tail) p->tail->next = t;
    else p->head = t;
    p->tail = t;
    p->queued++;
}

static void queue_unlink(AvpPool *p, Task *t) {
    if (t->prev) t->prev->next = t->next;
    else p->head = t->next;
    if (t->next) t->next->prev = t->prev;
    else p->tail = t->prev;
    t->next = t->prev = NULL;
    p->queued--;
}

typedef struct {
    AvpPool *pool;
    int index;
} WorkerArg;

static void worker_main(void *arg) {
    WorkerArg wa = *(WorkerArg *)arg;
    avp_xfree(arg);
    AvpPool *p = wa.pool;
    t_worker_index = wa.index;
    if (p->opts.thread_init) p->opts.thread_init(wa.index, p->opts.ctx);
    avp_mutex_lock(&p->mu);
    p->fresh--;
    for (;;) {
        while (!p->head && !p->shutdown) {
            p->idle++;
            avp_cond_wait(&p->work_cv, &p->mu, -1);
            p->idle--;
        }
        if (!p->head) break; /* shutdown and nothing left */
        Task *t = p->head;
        queue_unlink(p, t);
        if (t->map) t->map->running++;
        avp_mutex_unlock(&p->mu);
        t->run(t);
        avp_mutex_lock(&p->mu);
    }
    avp_mutex_unlock(&p->mu);
}

/* Starts workers while queued tasks outnumber the workers that are idle or just started and have not yet looked at
 * the queue, and there is room for more threads. Holds p->mu. */
static void spawn_locked(AvpPool *p) {
    while (p->nstarted < p->max_threads && (int)p->queued > p->idle + p->fresh) {
        if ((size_t)p->nstarted == p->threads_cap) {
            p->threads_cap = p->threads_cap ? p->threads_cap * 2 : 16;
            p->threads = avp_xrealloc(p->threads, p->threads_cap * sizeof *p->threads);
        }
        WorkerArg *wa = avp_xmalloc(sizeof *wa);
        wa->pool = p;
        wa->index = p->nstarted;
        p->fresh++;
        p->threads[p->nstarted++] = avp_thread_start(worker_main, wa, p->opts.stack_bytes);
    }
}

AvpPool *avp_pool_new_ex(const AvpPoolOpts *opts) {
    AvpPool *p = avp_xcalloc(1, sizeof *p);
    p->opts = *opts;
    p->max_threads = opts->threads > 0 ? opts->threads : avp_cpu_count();
    return p;
}

AvpPool *avp_pool_new(int threads) {
    AvpPoolOpts o = {0};
    o.threads = threads;
    return avp_pool_new_ex(&o);
}

void avp_pool_shutdown(AvpPool *p, bool cancel_pending) {
    avp_mutex_lock(&p->mu);
    p->shutdown = true;
    if (cancel_pending) {
        while (p->head) {
            Task *t = p->head;
            queue_unlink(p, t);
            if (t->future) avp_atomic_store32(&t->future->state, 3);
        }
    }
    avp_cond_broadcast(&p->work_cv);
    avp_cond_broadcast(&p->done_cv);
    int n = p->nstarted;
    avp_mutex_unlock(&p->mu);
    for (int i = 0; i < n; i++) avp_thread_join(p->threads[i]);
    avp_xfree(p->threads);
    avp_xfree(p);
}

/* The process-wide pool used when a map is given no pool of its own. */
static AvpPool *g_default;
static AvpOnce g_default_once;
static void make_default(void *ctx) {
    AVP_UNUSED(ctx);
    g_default = avp_pool_new(avp_cpu_count());
}
static AvpPool *default_pool(int need_threads) {
    avp_once(&g_default_once, make_default, NULL);
    avp_mutex_lock(&g_default->mu);
    if (g_default->max_threads < need_threads) g_default->max_threads = need_threads;
    avp_mutex_unlock(&g_default->mu);
    return g_default;
}

/* ---- map and parallel-for ---- */

static void job_fail(MapJob *j, size_t index) {
    for (;;) {
        i64 cur = avp_atomic_load64(&j->fail);
        if (cur != 0 && cur <= (i64)index + 1) return;
        if (avp_atomic_cas64(&j->fail, cur, (i64)index + 1)) return;
    }
}

static void job_run(MapJob *j) {
    for (;;) {
        if (avp_atomic_load64(&j->fail) != 0) return; /* an item failed: later ones are cancelled */
        size_t step = j->kind == JOB_RANGE ? j->grain : 1;
        size_t i = (size_t)avp_atomic_add64(&j->next, (i64)step) - step;
        if (i >= j->count) return;
        if (j->kind == JOB_RANGE) {
            size_t end = i + step < j->count ? i + step : j->count;
            j->rangefn(j->ctx, i, end);
        } else if (j->kind == JOB_FOR) {
            if (j->forfn(j->ctx, i) != 0) job_fail(j, i);
        } else {
            const void *item = j->items ? j->items + i * j->item_size : NULL;
            void *res = j->results ? j->results + i * j->result_size : NULL;
            if (j->mapfn(j->ctx, i, item, res) != 0) job_fail(j, i);
        }
    }
}

static void helper_run(Task *t) {
    MapJob *j = t->map;
    AvpPool *p = j->pool;
    job_run(j);
    avp_mutex_lock(&p->mu);
    j->running--;
    avp_cond_broadcast(&p->done_cv);
    avp_mutex_unlock(&p->mu);
}

static i64 run_job(AvpPool *pool, int nthreads, MapJob *j) {
    if (nthreads <= 0) nthreads = pool ? pool->max_threads : avp_cpu_count();
    if (pool && nthreads > pool->max_threads) nthreads = pool->max_threads; /* max_workers is a limit */
    j->next.v = 0;
    j->fail.v = 0;
    size_t units = j->kind == JOB_RANGE ? (j->count + j->grain - 1) / j->grain : j->count;
    size_t helpers = nthreads > 1 && units > 1 ? (size_t)(nthreads - 1) : 0;
    if (helpers > units - 1) helpers = units - 1;
    if (helpers == 0) {
        job_run(j);
        return j->kind == JOB_RANGE ? -1 : (i64)avp_atomic_load64(&j->fail) - 1;
    }
    if (!pool) pool = default_pool(nthreads - 1);
    j->pool = pool;
    j->running = 0;
    Task *tasks = avp_xcalloc(helpers, sizeof *tasks);
    avp_mutex_lock(&pool->mu);
    for (size_t k = 0; k < helpers; k++) {
        tasks[k].run = helper_run;
        tasks[k].map = j;
        queue_push(pool, &tasks[k]);
    }
    spawn_locked(pool);
    avp_cond_broadcast(&pool->work_cv);
    avp_mutex_unlock(&pool->mu);
    job_run(j); /* the calling thread works too */
    /* Helpers that never started are of no use now: take them back, then wait for the ones executing. */
    avp_mutex_lock(&pool->mu);
    for (size_t k = 0; k < helpers; k++) {
        Task *t = &tasks[k];
        if (t->next || t->prev || pool->head == t) queue_unlink(pool, t);
    }
    while (j->running > 0) avp_cond_wait(&pool->done_cv, &pool->mu, -1);
    avp_mutex_unlock(&pool->mu);
    avp_xfree(tasks);
    return j->kind == JOB_RANGE ? -1 : (i64)avp_atomic_load64(&j->fail) - 1;
}

i64 avp_pool_map(AvpPool *pool, int nthreads, AvpMapFn fn, void *ctx, const void *items, size_t count,
                 size_t item_size, void *results, size_t result_size) {
    if (count == 0) return -1;
    MapJob j = {0};
    j.kind = JOB_MAP;
    j.mapfn = fn;
    j.ctx = ctx;
    j.items = items;
    j.results = results;
    j.count = count;
    j.item_size = item_size;
    j.result_size = result_size;
    return run_job(pool, nthreads, &j);
}

i64 avp_pool_for(AvpPool *pool, int nthreads, AvpForFn fn, void *ctx, size_t count) {
    if (count == 0) return -1;
    MapJob j = {0};
    j.kind = JOB_FOR;
    j.forfn = fn;
    j.ctx = ctx;
    j.count = count;
    return run_job(pool, nthreads, &j);
}

void avp_pool_for_range(AvpPool *pool, int nthreads, AvpRangeFn fn, void *ctx, size_t count, size_t grain) {
    if (count == 0) return;
    MapJob j = {0};
    j.kind = JOB_RANGE;
    j.rangefn = fn;
    j.ctx = ctx;
    j.count = count;
    j.grain = grain ? grain : 1;
    run_job(pool, nthreads, &j);
}

/* ---- futures ---- */

static void future_run(Task *t) {
    AvpFuture *f = t->future;
    AvpPool *p = f->pool;
    if (!avp_atomic_cas32(&f->state, 0, 1)) return; /* cancelled while queued */
    f->result = f->fn(f->arg);
    AvpCompletions *q = f->done_q;
    if (q) { /* queued before the state flips: the consumer's result() then waits the instant that takes */
        avp_mutex_lock(&q->mu);
        f->next_done = NULL;
        if (q->tail) q->tail->next_done = f;
        else q->head = f;
        q->tail = f;
        avp_cond_signal(&q->cv);
        avp_mutex_unlock(&q->mu);
    }
    avp_mutex_lock(&p->mu);
    avp_atomic_store32(&f->state, 2); /* the last touch of f */
    avp_cond_broadcast(&p->done_cv);
    avp_mutex_unlock(&p->mu);
}

AvpFuture *avp_pool_submit_to(AvpPool *p, int (*fn)(void *arg), void *arg, AvpCompletions *done) {
    AvpFuture *f = avp_xcalloc(1, sizeof *f);
    f->pool = p;
    f->fn = fn;
    f->arg = arg;
    f->done_q = done;
    f->task.run = future_run;
    f->task.future = f;
    avp_mutex_lock(&p->mu);
    AVP_ASSERT(!p->shutdown);
    queue_push(p, &f->task);
    spawn_locked(p);
    avp_cond_signal(&p->work_cv);
    avp_mutex_unlock(&p->mu);
    return f;
}

AvpFuture *avp_pool_submit(AvpPool *p, int (*fn)(void *arg), void *arg) {
    return avp_pool_submit_to(p, fn, arg, NULL);
}

bool avp_future_done(AvpFuture *f) { return avp_atomic_load32(&f->state) >= 2; }
bool avp_future_cancelled(AvpFuture *f) { return avp_atomic_load32(&f->state) == 3; }

bool avp_future_result(AvpFuture *f, double timeout, int *result) {
    AvpPool *p = f->pool;
    if (avp_atomic_load32(&f->state) < 2) {
        avp_mutex_lock(&p->mu);
        double t0 = timeout >= 0 ? avp_monotonic() : 0;
        while (avp_atomic_load32(&f->state) < 2) {
            double left = -1;
            if (timeout >= 0) {
                double used = avp_monotonic() - t0;
                left = timeout - used;
                if (left <= 0) break;
            }
            avp_cond_wait(&p->done_cv, &p->mu, left);
        }
        avp_mutex_unlock(&p->mu);
    }
    i32 st = avp_atomic_load32(&f->state);
    if (st == 2) {
        if (result) *result = f->result;
        return true;
    }
    return false;
}

bool avp_future_cancel(AvpFuture *f) {
    /* A future that is running, done or cancelled is final here: its pool is not touched (it may be shut down and
       freed already: a future may outlive its pool once it has finished, as after avp_pool_shutdown). */
    if (avp_atomic_load32(&f->state) != 0) return false;
    AvpPool *p = f->pool;
    avp_mutex_lock(&p->mu);
    bool ok = false;
    if (avp_atomic_cas32(&f->state, 0, 3)) {
        if (f->task.next || f->task.prev || p->head == &f->task) queue_unlink(p, &f->task);
        avp_cond_broadcast(&p->done_cv);
        ok = true;
    }
    avp_mutex_unlock(&p->mu);
    return ok;
}

void avp_future_free(AvpFuture *f) {
    /* A running task still holds the struct, so a free waits for it (a task still queued is cancelled first). */
    avp_future_cancel(f);
    avp_future_result(f, -1, NULL);
    avp_xfree(f);
}

AvpCompletions *avp_completions_new(void) { return avp_xcalloc(1, sizeof(AvpCompletions)); }

AvpFuture *avp_completions_next(AvpCompletions *c, double timeout) {
    avp_mutex_lock(&c->mu);
    double t0 = avp_monotonic();
    while (!c->head) {
        double left = -1;
        if (timeout >= 0) {
            left = timeout - (avp_monotonic() - t0);
            if (left <= 0) {
                avp_mutex_unlock(&c->mu);
                return NULL;
            }
        }
        avp_cond_wait(&c->cv, &c->mu, left);
    }
    AvpFuture *f = c->head;
    c->head = f->next_done;
    if (!c->head) c->tail = NULL;
    avp_mutex_unlock(&c->mu);
    /* The worker queues f before it flips the state to done (the flip is its last touch of f), so wait for that
     * instant here: like as_completed, every future handed out is done and result(f, 0) succeeds. */
    avp_future_result(f, -1, NULL);
    return f;
}

void avp_completions_free(AvpCompletions *c) { avp_xfree(c); }

/* ---- the compiler fan-out ---- */

typedef struct {
    const AvpProcSpec *specs;
    AvpProcDoneFn done;
    void *ctx;
    AvpProcResult *results;
    AvpAtomic64 started_ok;
} ProcFan;

static int procfan_one(void *ctx, size_t index) {
    ProcFan *f = ctx;
    AvpProcResult r = avp_run(&f->specs[index]);
    if (r.status != AVP_PROC_START_FAILED) avp_atomic_add64(&f->started_ok, 1);
    if (f->results) f->results[index] = r;
    if (f->done) f->done(f->ctx, index, &r);
    return 0;
}

size_t avp_procpool_run(int max_parallel, const AvpProcSpec *specs, size_t n, AvpProcDoneFn done, void *ctx,
                        AvpProcResult *results) {
    ProcFan f = {specs, done, ctx, results, {0}};
    avp_pool_for(NULL, max_parallel, procfan_one, &f, n);
    return (size_t)avp_atomic_load64(&f.started_ok);
}
