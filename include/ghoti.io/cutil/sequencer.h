/**
 * @file
 *
 * A sequencer (reorder buffer): items go in, are finished in any order, and
 * come back out in the order they went in.
 *
 * The design and the reasoning behind each decision are recorded in
 * `documentation/sequencer.md`.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#ifndef GHOTI_IO_GCU_SEQUENCER_H
#define GHOTI_IO_GCU_SEQUENCER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <ghoti.io/cutil/allocator.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/mutex.h>
#include <ghoti.io/cutil/semaphore.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Passed as `capacity` to leave the number of outstanding items unbounded.
 */
#define GCU_SEQUENCER_UNBOUNDED ((size_t)0)

/**
 * The outcome of a submission or a retrieval.
 */
typedef enum GCU_Sequencer_Result {
  GCU_SEQUENCER_OK = 0,      ///< Succeeded.
  GCU_SEQUENCER_EMPTY,       ///< Nothing is outstanding; nothing to wait for.
  GCU_SEQUENCER_FULL,        ///< At capacity; collect a result and retry.
  GCU_SEQUENCER_NOT_READY,   ///< Outstanding, but the next one is unfinished.
  GCU_SEQUENCER_SHUTDOWN,    ///< Teardown has begun; the object is closing.
  GCU_SEQUENCER_ERROR,       ///< Bad argument, or out of memory.
} GCU_Sequencer_Result;

/**
 * One item held by the sequencer.
 */
typedef struct GCU_Sequencer_Slot {
  void * payload; ///< Private.  The caller's pointer, never dereferenced.
  int status;     ///< Private.  Whatever gcu_sequencer_complete() was given.
  bool used;      ///< Private.  A ticket has been issued for this slot.
  bool done;      ///< Private.  gcu_sequencer_complete() has been called.
} GCU_Sequencer_Slot;

/**
 * A sequencer.
 *
 * The layout is published only so that a caller may embed one in a structure
 * of its own and use the `_in_place` calls.  **Every field is private.**
 * Read the counts through gcu_sequencer_count_outstanding() and its
 * siblings, which take the lock; reading a field directly races the workers.
 */
typedef struct GCU_Sequencer GCU_Sequencer;

/// @cond HIDDEN_SYMBOLS
struct GCU_Sequencer {
  const GCU_Allocator * allocator; ///< Private.  Allocator for all memory.
  size_t capacity;                 ///< Private.  Item limit; 0 = unbounded.

  GCU_MUTEX_T mutex;               ///< Private.  Covers every field below.
  GCU_Sequencer_Slot * ring;       ///< Private.  Slots, indexed modulo size.
  size_t ring_size;                ///< Private.  Allocated slots; a power of 2.
  uint64_t next_in;                ///< Private.  Ticket the next submit gets.
  uint64_t next_out;               ///< Private.  Ticket the next collect wants.
  bool shutting_down;              ///< Private.  No new work accepted.
  size_t space_waiters;            ///< Private.  Threads awaiting a free slot.
  size_t result_waiters;           ///< Private.  Threads awaiting a result.

  GCU_Semaphore space;             ///< Private.  Posted when a slot frees.
  GCU_Semaphore ready;             ///< Private.  Posted when the head is done.
};
/// @endcond

/**
 * How a sequencer is created.
 *
 * Zero-initialising this structure asks for an unbounded sequencer using the
 * default allocator, which is the common case.
 */
typedef struct GCU_Sequencer_Config {
  /**
   * The greatest number of items that may be outstanding at once, or
   * ::GCU_SEQUENCER_UNBOUNDED for no limit.
   *
   * A bound is what makes gcu_sequencer_submit() able to report
   * ::GCU_SEQUENCER_FULL, and is how a producer is kept from running
   * arbitrarily far ahead of a consumer.  The slots are not all allocated up
   * front; the ring grows on demand and never exceeds this.
   */
  size_t capacity;

  /**
   * The allocator for every allocation the sequencer makes, or `NULL` for
   * gcu_allocator_default().
   */
  const GCU_Allocator * allocator;
} GCU_Sequencer_Config;

/**
 * Create a sequencer.
 *
 * @param config How to create it, or `NULL` for an unbounded sequencer on
 *   the default allocator.
 * @return The new sequencer, or `NULL` on failure.  Release it with
 *   gcu_sequencer_destroy().
 */
GCU_API GCU_Sequencer * gcu_sequencer_create(
  const GCU_Sequencer_Config * config);

/**
 * Create a sequencer in memory the caller owns.
 *
 * @param sequencer Storage for the sequencer.
 * @param config As gcu_sequencer_create().
 * @return `true` on success.  Release it with
 *   gcu_sequencer_destroy_in_place().
 */
GCU_API bool gcu_sequencer_create_in_place(
  GCU_Sequencer * sequencer, const GCU_Sequencer_Config * config);

/**
 * Shut a sequencer down and free it.
 *
 * Performs gcu_sequencer_shutdown(), waits for every released thread to
 * leave the sequencer's memory, and then frees it.
 *
 * Items still outstanding are dropped.  Their payloads belong to the caller,
 * which is the only party that knows how to release them.
 *
 * @param sequencer The sequencer, or `NULL`.
 */
GCU_API void gcu_sequencer_destroy(GCU_Sequencer * sequencer);

/**
 * As gcu_sequencer_destroy(), for a sequencer created with
 * gcu_sequencer_create_in_place().  The storage itself is the caller's.
 *
 * @param sequencer The sequencer, or `NULL`.
 */
GCU_API void gcu_sequencer_destroy_in_place(GCU_Sequencer * sequencer);

/**
 * Refuse further submissions and release every blocked thread.
 *
 * Threads blocked in gcu_sequencer_submit_wait() or gcu_sequencer_next()
 * return ::GCU_SEQUENCER_SHUTDOWN, as does every later call.  The sequencer
 * remains a valid object and must still be destroyed.
 *
 * This is how a caller that has abandoned its work unblocks a collector
 * waiting on a result that is never coming.  Calling it more than once is
 * harmless.
 *
 * @param sequencer The sequencer, or `NULL`.
 */
GCU_API void gcu_sequencer_shutdown(GCU_Sequencer * sequencer);

/**
 * Submit an item, refusing rather than waiting if the sequencer is full.
 *
 * **This is the call to reach for.**  A caller that collects its own results
 * must use this one; see gcu_sequencer_submit_wait() for why.
 *
 * The payload is stored and never dereferenced.  It must stay valid until
 * gcu_sequencer_next() returns it.
 *
 * @param sequencer The sequencer.
 * @param payload The caller's pointer, which may be `NULL`.
 * @param ticket_out Receives the ticket identifying this item, which is what
 *   gcu_sequencer_complete() takes.  May not be `NULL`.
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_FULL if the sequencer is at
 *   capacity, ::GCU_SEQUENCER_SHUTDOWN, or ::GCU_SEQUENCER_ERROR.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_submit(
  GCU_Sequencer * sequencer, void * payload, uint64_t * ticket_out);

/**
 * Submit an item, waiting for a free slot if the sequencer is full.
 *
 * **This deadlocks a caller that collects its own results.**  Space in a
 * bounded sequencer is freed by gcu_sequencer_next() and by nothing else, so
 * a thread that blocks here is waiting for space that only it could free.
 * Use this only when a *different* thread collects; otherwise use
 * gcu_sequencer_submit(), and collect a result when told the sequencer is
 * full.
 *
 * An unbounded sequencer never blocks here.
 *
 * @param sequencer The sequencer.
 * @param payload As gcu_sequencer_submit().
 * @param ticket_out As gcu_sequencer_submit().
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_SHUTDOWN if teardown began
 *   while waiting, or ::GCU_SEQUENCER_ERROR.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_submit_wait(
  GCU_Sequencer * sequencer, void * payload, uint64_t * ticket_out);

/**
 * Report that the work for a ticket has finished.
 *
 * Safe to call from any thread, and the usual caller is whichever worker ran
 * the item.  Each ticket must be completed exactly once; every submitted
 * ticket must eventually be completed, or a collector waits forever.
 *
 * @param sequencer The sequencer.
 * @param ticket The ticket from gcu_sequencer_submit().
 * @param status The item's result.  `0` is conventionally success.  The
 *   sequencer does not interpret it, and hands it back from
 *   gcu_sequencer_next().
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_SHUTDOWN, or
 *   ::GCU_SEQUENCER_ERROR if the ticket was never issued, has already been
 *   collected, or has already been completed.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_complete(
  GCU_Sequencer * sequencer, uint64_t ticket, int status);

/**
 * Collect the next item in submission order, waiting for it to finish.
 *
 * Blocks until the oldest uncollected item has been completed, however many
 * later items finished first.
 *
 * It waits for a *completion*, never for a *submission*.  With nothing
 * outstanding there is no completion that could ever arrive, so an empty
 * sequencer returns ::GCU_SEQUENCER_EMPTY at once rather than blocking on a
 * submission it has no reason to expect.  That is what makes EMPTY usable as
 * an end-of-stream signal by a caller that does not count its own items; the
 * price is that a collector which can outrun its producer must treat EMPTY as
 * "not yet" and try again.
 *
 * @param sequencer The sequencer.
 * @param payload_out Receives the payload, or `NULL` if not wanted.
 * @param status_out Receives the status given to gcu_sequencer_complete(),
 *   or `NULL` if not wanted.
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_EMPTY if nothing at all is
 *   outstanding, ::GCU_SEQUENCER_SHUTDOWN, or ::GCU_SEQUENCER_ERROR.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_next(
  GCU_Sequencer * sequencer, void ** payload_out, int * status_out);

/**
 * As gcu_sequencer_next(), but never waits.
 *
 * @param sequencer The sequencer.
 * @param payload_out As gcu_sequencer_next().
 * @param status_out As gcu_sequencer_next().
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_NOT_READY if items are
 *   outstanding but the oldest is unfinished, ::GCU_SEQUENCER_EMPTY,
 *   ::GCU_SEQUENCER_SHUTDOWN, or ::GCU_SEQUENCER_ERROR.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_try_next(
  GCU_Sequencer * sequencer, void ** payload_out, int * status_out);

/**
 * Discard every outstanding item and restart ticket numbering at zero.
 *
 * Intended for reusing a sequencer across independent runs.  Payloads are
 * the caller's to release, as ever.
 *
 * @param sequencer The sequencer.
 * @return ::GCU_SEQUENCER_OK, ::GCU_SEQUENCER_SHUTDOWN, or
 *   ::GCU_SEQUENCER_ERROR.
 */
GCU_API GCU_Sequencer_Result gcu_sequencer_reset(GCU_Sequencer * sequencer);

/**
 * How many items have been submitted but not yet collected.
 *
 * @param sequencer The sequencer, or `NULL`.
 * @return The count, or `0`.
 */
GCU_API size_t gcu_sequencer_count_outstanding(
  const GCU_Sequencer * sequencer);

/**
 * The configured capacity.
 *
 * @param sequencer The sequencer, or `NULL`.
 * @return The capacity, or `0` for unbounded.
 */
GCU_API size_t gcu_sequencer_capacity(const GCU_Sequencer * sequencer);

/**
 * Whether gcu_sequencer_next() would return an item without waiting.
 *
 * @param sequencer The sequencer, or `NULL`.
 * @return `true` if the oldest uncollected item has been completed.
 */
GCU_API bool gcu_sequencer_is_ready(const GCU_Sequencer * sequencer);

/**
 * Whether teardown has begun.
 *
 * @param sequencer The sequencer, or `NULL`.
 * @return `true` once gcu_sequencer_shutdown() or a destroy has been called.
 */
GCU_API bool gcu_sequencer_is_shutting_down(const GCU_Sequencer * sequencer);

#ifdef __cplusplus
}
#endif

#endif // GHOTI_IO_GCU_SEQUENCER_H
