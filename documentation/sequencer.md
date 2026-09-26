# Sequencer

**Status:** Implemented.  `include/ghoti.io/cutil/sequencer.h`, `src/sequencer.c`.

## 1. What it is

A sequencer accepts items in one order and hands them back in that same
order, no matter what order they were *finished* in.

Submitting an item returns a **ticket**: a monotonically increasing sequence
number.  Work happens wherever the caller likes -- typically on a `GCU_Pool`
-- and may finish in any order at all.  The worker reports each item done by
its ticket.  A collector then asks for items one at a time and receives them
strictly in submission order, blocking while the next one in line is still
outstanding.

In processor design this structure is a *reorder buffer*; the name here is
shortened to "sequencer" only because `gcu_sequencer_submit()` reads better
than `gcu_reorder_buffer_submit()`.  They are the same thing.

It pairs with `GCU_Pool` and does not overlap it.  The pool decides *when*
work runs; the sequencer decides *what order results are seen in*.  Neither
needs the other, and using both is the common case:

```
producer --submit--> [sequencer] <--complete-- workers <--task-- [pool]
                          |
                       next() --> consumer, in order
```

## 2. Prior art

`compress` had one, as `gcomp_job_queue_t`.  **Those files no longer exist**
-- `src/core/job_queue.c` and `include/ghoti.io/compress/job_queue.h` were
deleted when that library was converted to this module (section 10), so the
references below point at code that is only in its history.  They are kept
because the defects are the specification this module was written against.

It worked, and its concurrency was sound after two fixes made in the same
session as the thread pool (a waiter *count* rather than a `bool`, and
releasing blocked waiters before teardown frees the memory they are sleeping
on).  Four things were wrong with it as a *general* facility, and all four
are fixed here:

1. **The element type was not generic.**  `gcomp_block_job_t` carried the
   ordering fields (`sequence_num`, `status`) welded to compression payload
   (`input`, `output`, `output_capacity`, `output_size`).  Callers in
   `lz4_parallel.h` and `zstd_parallel.h` embedded it as a first member and
   downcast, so the queue could only ever hold that one struct.  Here the
   payload is a `void *` the sequencer never reads.

2. **The ticket lived in the caller's struct.**  `complete()` took a job
   pointer and read `job->sequence_num` out of it, which is why the element
   type had to be the queue's own.  Here `submit()` hands the ticket back and
   `complete()` takes it by value; the sequencer needs nothing from the
   payload.

3. **Lookup was a list walk.**  `find_slot()` scanned a linked list for a
   sequence number on every completion and every retrieval -- O(n) each, so
   O(n^2) over a stream, with a `calloc`/`free` per item on top.  Here slots
   live in a ring indexed by `ticket - next_out`, which is O(1), because
   retrieval in order means every outstanding ticket is in
   `[next_out, next_in)` by construction.

4. **"No items outstanding" was reported as an argument error.**
   `get_next_result()` returned `GCOMP_ERR_INVALID_ARG` for an empty queue,
   which is indistinguishable from a genuine misuse.  Here it is
   `GCU_SEQUENCER_EMPTY`, a distinct outcome.

## 3. The blocking-submit trap

A bounded sequencer frees space in exactly one place: `gcu_sequencer_next()`.
Nothing else.  So `gcu_sequencer_submit_wait()` is safe only when a
*different* thread collects.  A caller that both submits and collects -- the
usual single-threaded shape, since ordered results are usually wanted by
whoever produced them -- deadlocks the instant the buffer fills: it blocks
waiting for space that only it could free.

This is a real trap, and `compress` documented its way around it by warning
callers off its own function rather than giving them a working one.  The
answer here is that the *non*-blocking call is the plainly named one:

- `gcu_sequencer_submit()` refuses when full and returns
  `GCU_SEQUENCER_FULL`.  A single-threaded caller collects a result and
  retries.  This is the one to reach for.
- `gcu_sequencer_submit_wait()` blocks.  Its documentation says, in the first
  line, that it deadlocks a caller that collects its own results.

The naming follows `gcu_pool_enqueue()` / `gcu_pool_enqueue_wait()`, which
splits the same way for the same reason.

## 4. Capacity

`capacity == 0` means unbounded; any other value bounds the number of
outstanding items.  The ring starts small and doubles on demand, never past
`capacity`, so a bounded sequencer created with a capacity of a million does
not allocate a million slots up front, and an unbounded one still costs one
allocation per doubling rather than one per item.

## 5. Shutdown

`gcu_sequencer_shutdown()` refuses further submissions, releases every
blocked thread with `GCU_SEQUENCER_SHUTDOWN`, and leaves the object valid.
It exists so that a caller who has hit an error can unblock a collector that
is waiting for a result which is never coming.  It is idempotent.

`gcu_sequencer_destroy()` does that, then waits for every released thread to
leave the object's memory before freeing any of it, then frees.  The waiting
step is not optional: a thread released from a semaphore still has to
re-acquire the mutex on its way out, and freeing the mutex underneath it is a
use-after-free.  This is the same drain that `gcu_pool_destroy()` performs,
and it was a real bug in the pool before it was written.

Items still outstanding at destroy are simply dropped.  Their payloads belong
to the caller, which is the only party that knows how to release them; the
sequencer never owned them and cannot free them.

## 6. Waiting for a completion, not for a submission

`gcu_sequencer_next()` blocks until the oldest uncollected item is finished.
It does *not* block when there is nothing outstanding at all: with no items
in flight, no completion can ever arrive, and waiting would mean waiting on a
submission the sequencer was never told to expect.  An empty sequencer
returns `GCU_SEQUENCER_EMPTY` immediately.

That is a deliberate trade, and it cuts both ways:

- A caller that does not count its own items gets a usable end-of-stream
  signal.  Collect until EMPTY and you have everything.
- A collector that can outrun its producer must treat EMPTY as "not yet" and
  try again, rather than assuming it means "done".

The second half caught the author while writing `test-sequencer.cpp`, which
is fair warning that it will catch someone else, so it is stated on
`gcu_sequencer_next()` itself and not only here.  The alternative -- blocking
until *something* is submitted -- turns a caller that collects one item too
many into a silent hang, and trades a visible wart for an invisible one.

## 7. What it does not do

- **It does not run anything.**  No threads, no tasks.  Use a `GCU_Pool`.
- **It does not own payloads.**  A `void *` goes in and comes back out
  unexchanged.  The caller keeps it alive from `submit()` until `next()`
  returns it.
- **It does not cancel.**  Every submitted ticket must be completed, or the
  collector waits forever.  Erroring out is done by completing the ticket
  with a non-zero status.
- **It does not interpret status.**  `0` is conventionally success, and
  `gcu_sequencer_next()` hands the value back untouched.  What non-zero means
  is the caller's business.

## 8. Threading

Every public call takes the object's mutex.  Any number of threads may
submit, complete, and collect concurrently.  Collecting from several threads
at once is allowed and well-defined -- each item goes to exactly one caller
-- though the order they *observe* is then interleaved, which usually defeats
the point.

The two waiting conditions are counting semaphores with an explicit waiter
count beside each.  A wake posts once per registered waiter, inside the same
critical section that reads the count.  A single post plus a `bool` was the
defect found in both `compress`'s job queue and its thread pool: with two
threads blocked, one was woken and the other slept forever.

## 9. Testing

`test/test-sequencer.cpp`.  The cases that matter are the ones that fail
against a naive implementation:

- Completion in reverse order still yields submission order.
- Two threads blocked on the same condition are both released (the `bool`
  bug).
- Destroy with a collector blocked on an item that never completes.
- Destroy with a producer blocked on a full buffer.
- A bounded ring that wraps several times over.
- Growth past the initial ring size, bounded and unbounded.
- `submit()` reports full rather than deadlocking a self-collecting caller.

Run under `make test`, `make test-asan`, and `make test-tsan`.

---

## 10. Converting `compress`

**Done**, in the same session this module was written.

`gcomp_job_queue_t` is gone.  `src/core/parallel_block.c` creates a
`GCU_Sequencer` beside the `GCU_Pool` it already had, and the public
`include/ghoti.io/compress/job_queue.h`, `src/core/job_queue.c`,
`tests/core/test_job_queue.cpp` and fourteen `namespace.h` entries are
deleted.

Three details of that conversion are worth recording, because they are the
sort of thing a second consumer will hit too.

**The job struct stayed behind.**  `gcomp_block_job_t` is embedded as a first
member by `lz4_parallel.h` and `zstd_parallel.h` and downcast, so it could
not be deleted with the queue.  It moved to a *private* header,
`src/core/block_job.h`:  no public header outside `job_queue.h` itself ever
referenced it, so the public API surface shrank rather than being preserved
for its own sake.  A caller embedding a payload type is normal; what was
wrong was the queue *requiring* that type.

**The ticket goes in the payload, not the sequencer.**  `parallel_block.c`
stores the ticket in the job's own `sequence_num` field, and the pool's
completion callback reads it back to name what it just finished.  That write
happens before the job is handed to the pool, because a worker may run it the
instant it is enqueued.

**Two contracts were preserved deliberately rather than inherited.**  This
module reports an empty buffer as `GCU_SEQUENCER_EMPTY` where the old queue
returned an argument error, and `gcu_sequencer_reset()` drops outstanding
items where the old queue refused.  `compress` wanted the old behavior in
both cases, so it translates EMPTY back and checks before resetting -- its
inline path reports those two situations the old way, and the two modes must
not diverge.  A library adopting this module fresh has no such obligation.

The allocator matters:  `gcomp_allocator_t` *is* `GCU_Allocator`, so the
sequencer is created with `compress`'s tracked allocator and its ring counts
against that library's memory limit exactly as the queue it replaced did.
Passing `NULL` would have quietly moved those bytes outside the limit.
