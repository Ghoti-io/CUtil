# Thread Pool

**Status:** Design, reviewed. No code exists yet. This document describes the
intended `gcu_pool_*` module and the decisions behind it. The four questions
left open in the first draft were settled on 2026-09-19 and are recorded in
*Decisions 6-9* and *Prerequisites*; implementation may begin.

## Purpose

A thread pool that owns a fixed set of worker threads and a queue of tasks,
so that callers can hand off work without managing thread lifetimes
themselves. It is the missing piece between `thread.h`, `mutex.h` and
`semaphore.h`, all of which already ship, and the parallel work that the
sibling libraries in the suite want to do.

Two implementations already exist in the suite, and both informed this design:

- **`Ghoti.io/Pool`** - a C++ library, now superseded. Its API shape is worth
  reading; its implementation should not be ported.
- **`compress/src/core/thread_pool.c`** - a working C implementation built on
  cutil's own primitives. Much of it is right, and this design keeps its good
  decisions. The defects listed under *Prior Art* are the reason we are not
  simply moving that file into cutil unchanged.

This module replaces both. `compress` will migrate to it.

---

## 1. Scope

**In scope.** A pool of N worker threads; an unbounded FIFO task queue;
submitting a task; waiting for submitted work to finish; collecting the first
error a task reported; orderly shutdown; an inline mode that runs tasks on the
calling thread for deterministic testing.

**Out of scope, deliberately.**

- **Ordered result collection.** `compress`'s `job_queue` returns results in
  submission order regardless of completion order, and applies backpressure
  through a bounded capacity. That is a compression-specific concern layered
  *on top of* a pool, and it stays in `compress`.
- **Dynamic resizing.** The pool's thread count is fixed at creation. See
  *Decision 4*.
- **Task cancellation.** See *Decision 5*.
- **Work stealing, task priorities, task dependencies.** No caller in the
  suite needs them, and each would change the queue's shape.

---

## 2. Prior art, and what it teaches

### 2.1 The C++ `Ghoti.io/Pool`

Its architecture follows from one requirement: `~Pool()` must not block. In
C++ a destructor runs whether the program is ready or not, so honoring that
required detached workers, a process-global thread registry, `shared_ptr`
state kept alive past the pool's own death, and a global manager thread with
three queues and `promise`/`future` plumbing to create, stop and join.

Three parts of that machine do not work:

1. **The stop path is inert.** `src/pool.cpp:129` stores a default-constructed
   (empty) stop functor into the registry and nothing ever assigns to it;
   line 140 guards on that functor being non-empty, so every entry pushed onto
   `globalThreadStopQueue` is dequeued and discarded. Separately, lines 126-128
   construct a `jthread`, `detach()` it, and let it leave scope - and
   `~jthread` only calls `request_stop()` while the thread is still joinable,
   which after `detach()` it is not. The `stop_source` dies with the object, so
   `token.stop_requested()` can never become true. Workers exit only via the
   `terminate` flag or the over-count check.
2. **Nothing is joined.** Line 157 claims "The thread will automatically join
   when it is erased here," but the registry maps a thread id to a
   `pair<function<void()>, vector<promise<void>>>`; there is no thread object
   in it, and the threads were detached. `joinGlobalPool()` waits on promises
   that a worker fulfils *before* returning from its loop, so "joined" means
   "a worker announced it was about to exit." The process can begin tearing
   down while that thread is still unwinding.
3. **There is no way to drain.** `stop()` abandons the queue; `join()` abandons
   it and waits. The library's own README example loses its third task and
   documents that as expected.

**Lesson.** The non-blocking-destructor requirement bought a guarantee it did
not deliver, at a very high price in complexity. In C there is no destructor,
so the requirement does not exist: teardown is an explicit call at a point the
caller chose, and it can simply block. Deleting that one requirement deletes
the manager thread, the detachment, the keep-alive state and the promise
plumbing together.

### 2.2 `compress`'s `thread_pool.c`

This one is built the way this document proposes to build it - cutil mutex,
cutil counting semaphore, cutil threads, a linked-list FIFO - and it works.
Its good decisions are kept here: an inline mode, tasks that return a status,
first-error aggregation, a completion callback, a config struct, an injected
allocator, and a carefully unwound failure path in `create`.

It has four defects, which are the specification for what the cutil version
must get right.

**(a) `destroy` documents draining and implements abandoning.** The header
says destroy "Waits for all pending jobs to complete, then shuts down worker
threads" (`thread_pool.h:79-80`). The implementation never waits. It sets
`shutdown = true`, posts the semaphore once per worker, joins, and then frees
whatever is still queued (`thread_pool.c:304-340`). The worker loop tests
`pool->shutdown` *before* it pops (`thread_pool.c:156-173`), so a woken worker
returns without touching the queue.

It is worse than a plain abandon, because it is racy. Queued tasks and the
shutdown wakeups are counts on the *same* semaphore and are indistinguishable
to a worker. If K tasks are pending when `destroy` is called, a worker may
consume a task's post and run it, or a shutdown post and exit. **How many of
the queued tasks run is a race,** and the ones that do not run are freed
without ever executing and without reporting an error.

**(b) `wait` deadlocks with two waiters.** `waiting` is a single `bool`, and
the completion path signals `jobs_complete` exactly once
(`thread_pool.c:136-138`). If two threads call `gcomp_thread_pool_wait()`,
both set the flag, both block, one wakes, and the other waits forever. Nothing
in the header says `wait` may only be called from one thread.

**(c) `wait` can return while work is outstanding.** The `waiting` flag stays
true from the moment `wait` sets it until the waiter re-acquires the mutex
after waking. A second "last task finished" transition inside that window
signals `jobs_complete` again, leaving a surplus count on the semaphore. The
*next* call to `wait` then consumes the stale count and returns immediately,
reporting success while tasks are still queued and running.

**(d) One worker thread means synchronous execution.** `num_threads <= 1`
selects inline mode (`thread_pool.c:231`), so a caller who asks for a single
worker gets a `submit` that blocks and runs the task on the calling thread.
For compress's own use that is a deliberate optimization; as a general-purpose
contract it is a trap, because `submit` silently stops being asynchronous at
one particular thread count.

`pool->shutdown` is also read and written as a plain `bool` from several
threads. As written the semaphore operations happen to order every read after
the write that matters, so it is not a live bug - but it is a data race by the
letter of C11, it will be reported by ThreadSanitizer, and it is one
refactor away from being real.

---

## 3. Decision 1: a semaphore, not a condition variable

A worker needs to sleep until a task is available or the pool is shutting
down. The C++ version used `std::condition_variable`; cutil has no condition
variable, and this design does not add one, because a counting semaphore
expresses a task queue more directly:

- **submit** - append the task under the mutex, then signal once.
- **worker** - wait on the semaphore, then pop one task under the mutex.
- **shutdown** - set the state, then signal once per worker, so that every
  worker is guaranteed one wakeup.

The semaphore's count *is* the number of outstanding wakeups, so there is no
predicate to evaluate, no lost-wakeup window, and no spurious-wakeup loop to
write correctly. `gcu_semaphore_timedwait()` is already available if idle
worker reaping is ever wanted.

A condition variable may still be worth adding to cutil on its own merits, for
broadcast and predicate waits. This module does not require it, and that
addition should not block this one.

**Consequence to respect.** Because task availability and shutdown share one
semaphore, a worker cannot tell the two apart from the wakeup alone. It must
decide by inspecting state under the mutex, and it must check the queue
*before* it honors shutdown. Defect (a) above is exactly what happens when
that order is reversed.

---

## 4. Decision 2: shutdown means drain, and abandoning is a separate call

This is the decision with the most consequence, so it is stated plainly.

**Drain** - stop accepting new tasks; run every task already in the queue;
then stop the workers. Nothing submitted is silently lost.

**Abandon** - stop accepting new tasks; discard whatever is still queued; let
the tasks already executing finish; then stop the workers.

The distinction only becomes visible when tasks are still queued at shutdown,
which is precisely when a pool is under load. The failure it prevents is not
theoretical: `compress` today promises drain in its header and implements a
racy abandon, so a caller that submits blocks and then destroys the pool loses
an unpredictable number of them, with no error returned anywhere. Whatever
those blocks were - a chunk of a compressed stream - is simply missing.

The reason to make **drain the default** is that it is the behavior a caller
gets right by accident. Forgetting to drain before teardown costs you
completed work and returns no error, and the loss is timing-dependent, so it
survives testing and appears under load in production. Forgetting that you
wanted to abandon costs you some waiting. One of those is a silent data-loss
bug and the other is a delay, so the safe one is the default and the other is
spelled out:

```c
gcu_pool_destroy(pool);       // drain: run everything queued, then stop
gcu_pool_abandon(pool);       // discard the queue, then stop
```

`gcu_pool_abandon()` is genuinely useful - a caller tearing down after an
error has no reason to compress the remaining blocks - so it is offered, named
so that nobody reaches it without meaning to, and documented as discarding
work.

Tasks *already executing* are never interrupted in either case. Interrupting a
running task is task cancellation, which is *Decision 5*.

---

## 5. Decision 3: inline mode is explicit

Inline mode - running each task on the calling thread at submit time - is
worth keeping. It makes tests deterministic, removes thread overhead where
parallelism will not pay, and gives a single code path to callers on systems
where threads are unavailable.

It is selected **explicitly**, by `thread_count == 0`, and never inferred:

| `thread_count` | Behavior                                             |
|----------------|------------------------------------------------------|
| `0`            | Inline. Tasks run on the calling thread at submit.    |
| `1`            | One worker thread. `enqueue` is asynchronous.        |
| `n > 1`        | `n` worker threads.                                  |

A caller asking for one worker gets one worker. `gcu_pool_is_inline()` reports
which mode is in effect, and `GCU_POOL_THREADS_AUTO` is a named constant for
"one per logical processor," resolved through
`gcu_thread_get_num_processors()` with a documented fallback when that returns
zero.

---

## 6. Decision 4: the thread count is fixed at creation

The C++ version's `setThreadCount()` leaked comparisons like
`threads.size() > targetThreadCount` into the worker's wait predicate, into
shutdown, and into the bookkeeping of which threads were alive. That is where
much of its complexity, and its double thread registry, came from.

No caller in the suite resizes a pool. The count is fixed at creation; if a
caller wants a different size, it creates a different pool. Resizing can be
added later behind a new call without changing anything specified here.

---

## 7. Decision 5: no task cancellation in v1

The C++ `Task` carried an optional second function, invoked at shutdown, so a
task spinning in a loop could be told to stop. It is the best idea in that
API, and it is deferred rather than rejected.

It is deferred because it interacts with drain: a cancellation callback is
meaningful for a *running* task, while drain is about *queued* ones, and the
combination needs its own design - what happens to a task that is cancelled
mid-run, whether it still reports a status, whether drain implies cancel.
Doing it badly is worse than not doing it, and nothing in the suite needs it
today. Long-running interruptible tasks should, for now, poll a flag the
caller owns and passes through the task's own context pointer.

---

## 8. API

```c
#include <ghoti.io/cutil/pool.h>

/** Resolve the thread count to one worker per logical processor. */
#define GCU_POOL_THREADS_AUTO ((size_t)-1)

typedef struct GCU_Pool GCU_Pool;

/** A unit of work.  Returns 0 on success, non-zero on failure. */
typedef int (*GCU_Pool_Task)(void * ctx);

/** Invoked after a task returns, on the worker that ran it. */
typedef void (*GCU_Pool_Complete)(void * ctx, int status, void * user_data);

typedef struct GCU_Pool_Config {
  size_t thread_count;                ///< 0 = inline; GCU_POOL_THREADS_AUTO.
  size_t max_queued;                  ///< 0 = unbounded (the default).
  const char * name_prefix;           ///< NULL = "gcu-pool".  See Decision 8.
  const GCU_Allocator * allocator;    ///< NULL = gcu_allocator_default().
} GCU_Pool_Config;

// Lifecycle.  `config` may be NULL for all defaults.
GCU_API GCU_Pool * gcu_pool_create(const GCU_Pool_Config * config);
GCU_API bool gcu_pool_create_in_place(
  GCU_Pool * pool, const GCU_Pool_Config * config);
GCU_API void gcu_pool_destroy(GCU_Pool * pool);           // drains
GCU_API void gcu_pool_destroy_in_place(GCU_Pool * pool);  // drains
GCU_API void gcu_pool_abandon(GCU_Pool * pool);           // discards queue
GCU_API void gcu_pool_abandon_in_place(GCU_Pool * pool);

// Work.  The plain forms never block; the _wait forms block for a free slot
// when the queue is bounded.  See Decision 6.
GCU_API bool gcu_pool_enqueue(GCU_Pool * pool, GCU_Pool_Task task, void * ctx);
GCU_API bool gcu_pool_enqueue_cb(GCU_Pool * pool, GCU_Pool_Task task,
  void * ctx, GCU_Pool_Complete on_complete, void * user_data);
GCU_API bool gcu_pool_enqueue_wait(
  GCU_Pool * pool, GCU_Pool_Task task, void * ctx);
GCU_API bool gcu_pool_enqueue_wait_cb(GCU_Pool * pool, GCU_Pool_Task task,
  void * ctx, GCU_Pool_Complete on_complete, void * user_data);

// Synchronization.  Safe to call from any number of threads at once.
GCU_API int  gcu_pool_wait(GCU_Pool * pool);   // block until idle; first error
GCU_API void gcu_pool_clear_error(GCU_Pool * pool);

// Introspection.
GCU_API size_t gcu_pool_count_queued(const GCU_Pool * pool);
GCU_API size_t gcu_pool_count_active(const GCU_Pool * pool);
GCU_API size_t gcu_pool_count_threads(const GCU_Pool * pool);
GCU_API bool   gcu_pool_is_inline(const GCU_Pool * pool);
GCU_API bool   gcu_pool_is_shutting_down(const GCU_Pool * pool);
```

Every name is added to `include/ghoti.io/cutil/namespace.h`, as the
`check-symbols` target requires.

### Contract notes

- **`gcu_pool_enqueue` never blocks** in threaded mode, and never runs the
  task on the caller's thread. In inline mode it runs the task to completion
  before returning. This is the one caller-visible difference between the
  modes, and it is why inline is explicit.
- **`gcu_pool_enqueue` returns `false`** on allocation failure, on a NULL
  task, if the pool is shutting down, or if the queue is bounded and full. It
  does not silently queue work that will never run.
- **`gcu_pool_enqueue_wait` blocks** until a slot frees, and returns `false`
  if the pool begins shutting down while it waits. On an unbounded queue it
  behaves exactly like `gcu_pool_enqueue`, since a slot is always available.
  **It must never be called from a task running on the same pool:** the slot
  it waits for can only be freed by a worker, and it is occupying one. That
  restriction is the price of blocking, and it is why the blocking form is a
  separate function rather than a mode of the ordinary one.
- **`gcu_pool_wait` returns the first non-zero status** reported by any task
  since the last `gcu_pool_clear_error()`, or `0`. Retrieving the error does
  not clear it; clearing is explicit, so that two threads calling `wait` see
  the same answer rather than racing to consume it. This is a deliberate
  departure from `compress`, where `wait` resets `first_error` as a side
  effect and the second caller is told everything succeeded.
- **`gcu_pool_wait` may be called concurrently** from any number of threads.
  All waiters are released together.
- **`gcu_pool_wait` returning means** the queue is empty and no task is
  executing *at the instant the condition was observed*. If other threads are
  still submitting, more work may exist by the time the caller looks. It is
  the caller's job to stop submitting first; the pool does not guess.
- **The completion callback runs on the worker thread**, outside every pool
  lock, immediately after the task returns and before the task is counted
  complete. It must not call back into the pool that invoked it.
- **Counts are observations, not guarantees.** `gcu_pool_count_queued()` and
  friends are accurate at the moment they sample and may be stale before they
  return. They are for diagnostics and tests.

---

## 9. Internal design

```c
struct GCU_Pool {
  const GCU_Allocator * allocator;
  size_t thread_count;
  GCU_Thread * threads;          // thread_count entries, NULL when inline
  bool is_inline;

  GCU_MUTEX_T mutex;             // covers every field below
  GCU_Array queue;               // FIFO of GCU_Pool_Item, head index below
  size_t queue_head;             // index of next item to run
  size_t active;                 // tasks currently executing
  int first_error;               // first non-zero status seen
  bool shutting_down;            // no new work accepted
  bool draining;                 // run out the queue before stopping

  size_t max_queued;             // 0 = unbounded
  GCU_Semaphore work;            // counts outstanding wakeups
  GCU_Semaphore idle;            // releases waiters when quiescent
  GCU_Semaphore slots;           // free queue slots; unused when unbounded
  size_t waiters;                // threads blocked in gcu_pool_wait()
  size_t slot_waiters;           // threads blocked in gcu_pool_enqueue_wait()
};
```

**The queue** is a `GCU_Array` of task records plus a head index, rather than
the linked list `compress` uses. `GCU_Array` is byte-oriented, so it stores
the record inline with no per-task allocation, which removes a malloc and a
free from the hot path and removes the failure mode where enqueueing fails
under memory pressure at the worst moment. The array is compacted when the
head passes the midpoint, so it does not grow without bound under steady load.

**One mutex, not two.** `compress` uses a `queue_mutex` and a `wait_mutex` and
must take both, in a documented order, in the two places that inspect
`pending_count` and `active_jobs` together. Since every such inspection needs
both anyway, the second mutex adds a lock-ordering rule and buys nothing. One
mutex is held only for queue manipulation and counter updates - never across a
task, a callback, or a semaphore wait - so contention is not a concern at the
task granularity this pool is for.

**`shutting_down` and `draining` are read under the mutex,** not as bare
`bool`s, which removes the data race noted in *Prior Art* without needing
`<stdatomic.h>` or a per-platform atomic shim.

### The worker loop

The order of the checks is the whole correctness argument, so it is written
out rather than left to the implementation:

```
loop:
  wait(work)                      // one wakeup per queued task or shutdown
  lock(mutex)
    if queue is non-empty:
      pop a task; active += 1
      unlock(mutex)
      status = task.fn(task.ctx)
      if task.on_complete: task.on_complete(task.ctx, status, task.user_data)
      lock(mutex)
        active -= 1
        if status != 0 and first_error == 0: first_error = status
        signal_idle_if_quiescent()
      unlock(mutex)
      continue loop
    // queue is empty
    if shutting_down:
      unlock(mutex); return
    unlock(mutex)
    continue loop                 // surplus wakeup; go back to sleep
```

**The queue is always checked before shutdown is honored.** That single
ordering is what makes drain correct and is the direct fix for defect (a).
A worker stops only when the queue is empty *and* shutdown has been requested,
so `gcu_pool_destroy` does not need to wait for the queue itself - it sets the
state, wakes every worker, and joins, and the workers drain it on the way out.

`gcu_pool_abandon` clears the queue under the mutex *before* waking the
workers, so they find it empty and exit. Abandon and drain therefore share one
shutdown path and differ by one line, rather than being two mechanisms.

### Waking waiters

`signal_idle_if_quiescent()` runs under the mutex and posts `idle` once for
**each** currently blocked waiter:

```
if queue is empty and active == 0 and waiters > 0:
  for i in 0 .. waiters-1: signal(idle)
  waiters = 0
```

Setting `waiters = 0` in the same critical section that posts is what prevents
defect (c): the surplus count can no longer be generated, because the
condition cannot fire twice for the same set of waiters. Posting once per
waiter rather than once is what prevents defect (b). A waiter increments
`waiters` under the mutex and releases it before blocking, so the count and
the post are always consistent.

### The bounded queue

When `max_queued` is zero the `slots` semaphore is never created and both
enqueue forms behave identically. When it is non-zero, `slots` starts at
`max_queued` and is the authority on free space:

- `gcu_pool_enqueue` takes a slot with `gcu_semaphore_trywait()` and returns
  `false` immediately if none is free.
- `gcu_pool_enqueue_wait` increments `slot_waiters` under the mutex, then
  blocks in `gcu_semaphore_wait()`.
- A worker posts `slots` **after** it pops a task and releases the mutex, so a
  slot frees as soon as the task leaves the queue rather than when it finishes.

**Shutdown must release blocked producers**, or teardown deadlocks against a
producer waiting for a slot that no worker will ever free. The shutdown path
therefore posts `slots` once per `slot_waiters` under the same mutex that sets
`shutting_down`, and a released producer re-checks `shutting_down` before
touching the queue and returns `false` if it is set. This is the same
count-and-post-together discipline used for `waiters`, and for the same
reason.

### Worker names

Workers are named `<prefix>-<index>` - `zstd-enc-0`, `zstd-enc-1` - from
`config.name_prefix`, defaulting to `gcu-pool`. `pthread_setname_np` accepts
at most 15 characters plus the terminator on Linux, so the prefix is truncated
to **8 characters** to leave room for the index; the limit is documented on
the config field. Windows' `SetThreadDescription` has no comparable limit, and
the same truncated name is used on both so that a thread is called the same
thing everywhere.

Naming is **best-effort**: `gcu_thread_set_name()` failures are ignored. A
diagnostic label is never a reason to fail a pool creation.

### Inline mode

Inline mode creates no threads and no semaphores. `gcu_pool_enqueue` runs the
task, invokes the callback, and records the error, all on the caller's thread.
`gcu_pool_wait` returns `first_error` immediately. `gcu_pool_destroy` frees
the structure. The mutex is still created and taken, so that an inline pool
shared between threads behaves, and so that there is one code path rather than
two.

---

## 10. Error handling

- Every entry point tolerates a `NULL` pool by returning a documented failure
  value, so that teardown after a failed create is always safe.
- `gcu_pool_create` unwinds precisely: each primitive that was created is
  destroyed on the failure path, in reverse order. If thread *i* of *n* fails
  to start, the *i* already-started threads are told to shut down, woken, and
  joined before anything is freed. `compress` gets this right today and the
  structure is worth keeping.
- A task's non-zero status is recorded, not acted on. The pool never stops
  early because a task failed; callers who want that behavior check the status
  in a completion callback and stop submitting.
- The pool does not catch anything. A task that crashes, or that calls
  `exit()`, takes the process with it - the same as any other C call.
- `size_t` arithmetic on counts uses `safemath.h` where a caller-supplied
  value is involved, notably the initial capacity derived from
  `thread_count`.

---

## 11. Testing

`test/test-pool.cpp`, gtest, in the style of `test-mutex.cpp` and
`test-semaphore.cpp`. Correctness here is mostly about orderings that a
single-threaded test cannot reach, so the suite is explicit about them:

**Basic.** Create and destroy, every thread count including `0`, `1` and
`GCU_POOL_THREADS_AUTO`. Enqueue one task, wait, confirm it ran. Enqueue many,
confirm each ran exactly once - a per-task flag array, not a counter, so a
double-run is caught.

**Drain.** Enqueue *N* slow tasks into a pool with fewer than *N* workers, so
tasks are certainly still queued, then `gcu_pool_destroy`. **Every task must
have run.** This is the direct regression test for the defect in `compress`,
and it should be written first and confirmed to fail against a worker loop
that checks shutdown before the queue.

**Abandon.** The same setup with `gcu_pool_abandon`. Tasks already running
complete; queued tasks do not run; nothing leaks. Run under ASan.

**Wait.** Concurrent `gcu_pool_wait` from several threads - all return, none
hangs (defect (b)). Wait, submit more, wait again - the second wait must not
return early (defect (c)). Wait on an idle pool returns immediately. Wait on
an inline pool returns immediately.

**Errors.** A failing task's status surfaces from `wait`. The *first* error is
kept when several fail. `clear_error` resets. Two waiters both observe the
error rather than one consuming it.

**Callbacks.** Fire once per task, receive the right status and `user_data`,
run before the task counts as complete.

**Contention.** Many threads enqueueing while many workers drain, with a
checksum over the results, repeated enough to shake out orderings.

**Bounded queue.** With `max_queued` set, `gcu_pool_enqueue` returns `false`
once full rather than growing. `gcu_pool_enqueue_wait` blocks while full and
proceeds when a worker frees a slot. A producer blocked in
`gcu_pool_enqueue_wait` is released and returns `false` when shutdown begins -
run this one under a watchdog, since the failure mode is a hang rather than an
assertion. An unbounded pool treats both forms identically.

**Worker names.** With a prefix set, `gcu_thread_get_name()` reports
`<prefix>-<index>`. An over-long prefix is truncated rather than failing the
create. A pool still works on a platform where naming fails.

**Lifecycle.** Enqueue after shutdown begins returns `false`. Destroy with an
empty queue. Destroy an inline pool. Destroy a pool that was never used. All
of the above under `make test-asan` (which runs UBSan in the same pass); the
contention and drain cases additionally under ThreadSanitizer, which will require adding a
`test-tsan` target alongside the existing sanitizer targets.

---

## 12. Migrating `compress`

`compress` already depends on cutil for threads, mutexes and semaphores, so
this is a substitution rather than a new dependency.

1. Land `gcu_pool_*` in cutil with its tests green.
2. Reimplement `gcomp_thread_pool_*` as a thin shim over `gcu_pool_*`,
   translating `gcomp_status_t` to and from the pool's `int`. `compress`'s
   existing `tests/core/test_thread_pool.cpp` must pass unchanged - it is the
   evidence that the substitution is faithful.
3. Fix the header's drain promise, which the shim now actually honors.
4. Convert the three consumers - `src/core/parallel_block.c`,
   `src/methods/zstd/zstd_parallel.c`, `src/methods/lz4/` - to call
   `gcu_pool_*` directly.
5. Delete `src/core/thread_pool.c` and its header, and remove their entries
   from `include/ghoti.io/compress/namespace.h`.

`job_queue` is untouched. It layers ordered, bounded result collection over a
pool and remains compress-specific.

**One behavior change to announce.** Any `compress` caller that relies today
on `gcomp_thread_pool_destroy` *not* running queued jobs will find that it now
does. Given the header has always documented draining, such a caller is
relying on a bug, but the change should be called out in the commit rather
than discovered.

---

## 13. Prerequisites

Two commits land in cutil ahead of the pool. Both are worth making on their
own merits, and the pool depends on each.

**1. `gcu_thread_get_num_processors()` must always return at least 1.**
The POSIX branch is `return sysconf(_SC_NPROCESSORS_ONLN);` with a return type
of `unsigned int`. `sysconf` returns `long` and yields `-1` on failure, which
converts to `UINT_MAX` rather than to zero, and `_SC_NPROCESSORS_ONLN` is a
glibc/BSD extension that genuinely can fail where `/proc` or `/sys` is not
available. This is already a live defect independent of the pool: the thread
module's constructor calls `gcu_hash64_create(gcu_thread_get_num_processors()
* 3)`, so on that path it asks for a table of nearly 2^32 entries, the
allocation fails, the constructor returns early leaving `gcu_thread_hash` as
`NULL`, and **every `gcu_thread_*` call fails for the life of the process**
with no way for the constructor to report it.

The fix captures the `long`, clamps anything below 1 to 1, and documents that
the function always returns a usable count. The Windows branch is clamped for
symmetry, though `dwNumberOfProcessors` is always at least 1 in practice.
`GCU_POOL_THREADS_AUTO` then needs no special case.

The same commit documents the length limit on `gcu_thread_set_name()`, which
currently promises nothing: the POSIX path is `pthread_setname_np`, which
fails with `ERANGE` above 15 characters plus the terminator, while Windows
accepts far more. A name that works on one platform silently fails on the
other, and the header should say so.

**2. A `test-tsan` target.** ThreadSanitizer is the tool that finds the class
of defect this module is most likely to have - it is exactly what would have
caught the unsynchronized `shutdown` flag in `compress`. The target mirrors
the existing `ASAN_TEST_RULE` structure with `-fsanitize=thread` and its own
object and application tree, since TSan and ASan cannot be combined.

It is driven by its own `TSAN_TEST_NAMES` list rather than by `TEST_NAMES`,
starting with `test-mutex`, `test-semaphore` and `test-thread`, and
`test-pool` joins the list as it is written so that the pool is developed
under TSan from its first commit. The narrower list is deliberate: `hash` and
`vector` ship a mutex that the README describes as the caller's
responsibility to use, so their tests may race by design, and an audit of
those modules should not gate this one. Anything pre-existing that TSan
reports gets its own issue.

---

## 14. Decisions settled during review

The first draft left four questions open. All four were answered on
2026-09-19, and the sections above reflect the answers. Recorded here so that
the reasoning is not lost:

**Decision 6 - the queue may be bounded, and blocking is a separate call.**
`max_queued` defaults to 0, meaning unbounded, so the simple case is
unchanged. When it is set, `gcu_pool_enqueue` fails rather than blocking and
`gcu_pool_enqueue_wait` blocks. Two names rather than one function whose
behavior flips on a config value, because inferring blocking from a number is
the same trap as `compress` inferring synchronous execution from
`num_threads == 1`. A caller reading the call site can see which one it gets.

**Decision 7 - the processor count is fixed at the primitive.** Rather than
having the pool defend against a broken return value, the broken return value
is fixed where it lives, which also repairs the thread-module constructor.
See *Prerequisites*.

**Decision 8 - worker names carry a caller-supplied prefix.** `name_prefix`
defaults to `gcu-pool`. The suite is about to run several pools at once -
`parallel_block`, zstd and lz4 - and `zstd-enc-3` identifies which pool a
thread belongs to where `gcu-pool-3` does not. Truncation to 8 characters is
documented; naming never fails a create.

**Decision 9 - `test-tsan` lands first, scoped.** See *Prerequisites*.
