/**
 * @file
 *
 * A sequencer: a reorder buffer built on this library's own mutexes and
 * semaphores.
 *
 * The design, and the reasoning behind each decision here, are recorded in
 * `documentation/sequencer.md`.
 *
 * Copyright 2026 by Corey Pennycuff
 */

#include <string.h>

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/sequencer.h>
#include <ghoti.io/cutil/thread.h>

/**
 * The number of slots a sequencer starts with, unless a smaller capacity
 * makes even that wasteful.
 */
#define GCU_SEQUENCER_INITIAL_RING 16

//
// Helpers.  Every one of these requires the mutex to be held.
//

/**
 * The number of items submitted but not yet collected.
 *
 * Retrieval is strictly in order, so every outstanding ticket lies in
 * `[next_out, next_in)` and this subtraction is the whole of the bookkeeping.
 */
static size_t gcu_sequencer_outstanding(const GCU_Sequencer * sequencer) {
  return (size_t)(sequencer->next_in - sequencer->next_out);
}

/**
 * The slot a ticket lives in.
 *
 * `ring_size` is always a power of two, so the modulo is a mask.
 */
static GCU_Sequencer_Slot * gcu_sequencer_slot(
  GCU_Sequencer * sequencer, uint64_t ticket) {
  return &sequencer->ring[(size_t)(ticket & (uint64_t)(sequencer->ring_size - 1))];
}

/**
 * Whether the oldest uncollected item has been completed.
 */
static bool gcu_sequencer_ready(GCU_Sequencer * sequencer) {
  if (gcu_sequencer_outstanding(sequencer) == 0) {
    return false;
  }

  GCU_Sequencer_Slot * slot = gcu_sequencer_slot(sequencer, sequencer->next_out);
  return slot->used && slot->done;
}

/**
 * Wake every thread registered as waiting on one condition.
 *
 * Posting once per registered waiter rather than once is the whole of the
 * point.  A single post wakes one thread and leaves the rest blocked
 * forever; that was a real defect in two hand-rolled queues before this
 * module existed.
 *
 * Must be called with the mutex held, so that the count cannot change
 * between reading it and posting.  Waking more threads than can proceed is
 * safe: every waiter re-tests its condition under the mutex and waits again
 * if it cannot continue.
 *
 * @param semaphore The condition to signal.
 * @param waiters How many threads are registered as waiting on it.
 */
static void gcu_sequencer_wake_all(GCU_Semaphore * semaphore, size_t waiters) {
  for (size_t i = 0; i < waiters; ++i) {
    gcu_semaphore_signal(semaphore);
  }
}

/**
 * Double the ring, re-seating every outstanding item at its new index.
 *
 * The index of a ticket depends on the ring size, so growing is a rehash and
 * not a `realloc`:  each live slot moves from its old mask to its new one.
 *
 * @param sequencer The sequencer.
 * @return `true` if the ring grew.
 */
static bool gcu_sequencer_grow(GCU_Sequencer * sequencer) {
  if (sequencer->ring_size > SIZE_MAX / 2) {
    return false;
  }

  size_t new_size = sequencer->ring_size * 2;
  GCU_Sequencer_Slot * ring = gcu_allocator_calloc(
    sequencer->allocator, new_size, sizeof(GCU_Sequencer_Slot));
  if (!ring) {
    return false;
  }

  size_t outstanding = gcu_sequencer_outstanding(sequencer);
  for (size_t i = 0; i < outstanding; ++i) {
    uint64_t ticket = sequencer->next_out + (uint64_t)i;
    ring[(size_t)(ticket & (uint64_t)(new_size - 1))] =
      *gcu_sequencer_slot(sequencer, ticket);
  }

  gcu_allocator_free(sequencer->allocator, sequencer->ring);
  sequencer->ring = ring;
  sequencer->ring_size = new_size;

  return true;
}

/**
 * Set every field of a fresh sequencer, allocating its ring.
 *
 * @param sequencer The storage to initialise.
 * @param config As gcu_sequencer_create().
 * @return `true` on success, having left nothing allocated on failure.
 */
static bool gcu_sequencer_init(
  GCU_Sequencer * sequencer, const GCU_Sequencer_Config * config) {
  const GCU_Allocator * allocator = config && config->allocator
    ? config->allocator
    : gcu_allocator_default();
  size_t capacity = config ? config->capacity : 0;

  // Start at the default unless the capacity is smaller, in which case round
  // it up to a power of two and stop there:  a sequencer bounded at three
  // items has no use for sixteen slots.
  size_t ring_size = GCU_SEQUENCER_INITIAL_RING;
  if (capacity && capacity < ring_size) {
    ring_size = 1;
    while (ring_size < capacity) {
      ring_size *= 2;
    }
  }

  memset(sequencer, 0, sizeof(GCU_Sequencer));
  sequencer->allocator = allocator;
  sequencer->capacity = capacity;

  sequencer->ring = gcu_allocator_calloc(
    allocator, ring_size, sizeof(GCU_Sequencer_Slot));
  if (!sequencer->ring) {
    return false;
  }
  sequencer->ring_size = ring_size;

  if (GCU_MUTEX_CREATE(sequencer->mutex) != 0) {
    gcu_allocator_free(allocator, sequencer->ring);
    sequencer->ring = NULL;
    return false;
  }

  if (gcu_semaphore_create(&sequencer->space, 0) != 0) {
    GCU_MUTEX_DESTROY(sequencer->mutex);
    gcu_allocator_free(allocator, sequencer->ring);
    sequencer->ring = NULL;
    return false;
  }

  if (gcu_semaphore_create(&sequencer->ready, 0) != 0) {
    gcu_semaphore_destroy(&sequencer->space);
    GCU_MUTEX_DESTROY(sequencer->mutex);
    gcu_allocator_free(allocator, sequencer->ring);
    sequencer->ring = NULL;
    return false;
  }

  return true;
}

/**
 * Shut down, wait for every released thread to leave, and free the ring.
 *
 * The waiting step is not optional.  A thread released from a semaphore
 * still has to re-acquire the mutex on its way out, so destroying the mutex
 * or freeing the ring the moment it is woken is a use-after-free.
 *
 * A released waiter decrements its count under the mutex and touches nothing
 * afterwards, so counts of zero observed while holding the mutex mean they
 * are all gone.  No new waiter can appear behind them:  registering requires
 * the mutex, and `shutting_down` is already set.
 *
 * @param sequencer The sequencer.
 */
static void gcu_sequencer_teardown(GCU_Sequencer * sequencer) {
  GCU_MUTEX_LOCK(sequencer->mutex);

  sequencer->shutting_down = true;
  gcu_sequencer_wake_all(&sequencer->ready, sequencer->result_waiters);
  gcu_sequencer_wake_all(&sequencer->space, sequencer->space_waiters);

  while (sequencer->result_waiters || sequencer->space_waiters) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    gcu_thread_yield();
    GCU_MUTEX_LOCK(sequencer->mutex);
  }

  gcu_allocator_free(sequencer->allocator, sequencer->ring);
  sequencer->ring = NULL;
  sequencer->ring_size = 0;

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  gcu_semaphore_destroy(&sequencer->ready);
  gcu_semaphore_destroy(&sequencer->space);
  GCU_MUTEX_DESTROY(sequencer->mutex);
}

//
// Public API.
//

GCU_Sequencer * gcu_sequencer_create(const GCU_Sequencer_Config * config) {
  const GCU_Allocator * allocator = config && config->allocator
    ? config->allocator
    : gcu_allocator_default();

  GCU_Sequencer * sequencer =
    gcu_allocator_malloc(allocator, sizeof(GCU_Sequencer));
  if (!sequencer) {
    return NULL;
  }

  if (!gcu_sequencer_init(sequencer, config)) {
    gcu_allocator_free(allocator, sequencer);
    return NULL;
  }

  return sequencer;
}

bool gcu_sequencer_create_in_place(
  GCU_Sequencer * sequencer, const GCU_Sequencer_Config * config) {
  if (!sequencer) {
    return false;
  }

  return gcu_sequencer_init(sequencer, config);
}

void gcu_sequencer_destroy(GCU_Sequencer * sequencer) {
  if (!sequencer || !sequencer->allocator) {
    return;
  }

  const GCU_Allocator * allocator = sequencer->allocator;

  gcu_sequencer_teardown(sequencer);
  gcu_allocator_free(allocator, sequencer);
}

void gcu_sequencer_destroy_in_place(GCU_Sequencer * sequencer) {
  if (!sequencer || !sequencer->allocator) {
    return;
  }

  gcu_sequencer_teardown(sequencer);
}

void gcu_sequencer_shutdown(GCU_Sequencer * sequencer) {
  if (!sequencer) {
    return;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);
  sequencer->shutting_down = true;
  gcu_sequencer_wake_all(&sequencer->ready, sequencer->result_waiters);
  gcu_sequencer_wake_all(&sequencer->space, sequencer->space_waiters);
  GCU_MUTEX_UNLOCK(sequencer->mutex);
}

/**
 * Add @p payload, waiting for a free slot only if @p blocking.
 *
 * Space is freed by gcu_sequencer_next() and by nothing else, so waiting
 * here is only safe when some OTHER thread collects.  See the warning on
 * gcu_sequencer_submit_wait().
 */
static GCU_Sequencer_Result gcu_sequencer_submit_internal(
  GCU_Sequencer * sequencer, void * payload, uint64_t * ticket_out,
  bool blocking) {
  if (!sequencer || !ticket_out) {
    return GCU_SEQUENCER_ERROR;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);

  if (sequencer->shutting_down) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_SHUTDOWN;
  }

  while (sequencer->capacity
      && gcu_sequencer_outstanding(sequencer) >= sequencer->capacity) {
    if (!blocking) {
      GCU_MUTEX_UNLOCK(sequencer->mutex);
      return GCU_SEQUENCER_FULL;
    }

    ++sequencer->space_waiters;
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    gcu_semaphore_wait(&sequencer->space);
    GCU_MUTEX_LOCK(sequencer->mutex);
    --sequencer->space_waiters;

    // Released by teardown rather than by a slot freeing.  Return without
    // touching anything else:  the ring is about to be freed.
    if (sequencer->shutting_down) {
      GCU_MUTEX_UNLOCK(sequencer->mutex);
      return GCU_SEQUENCER_SHUTDOWN;
    }
  }

  if (gcu_sequencer_outstanding(sequencer) == sequencer->ring_size
      && !gcu_sequencer_grow(sequencer)) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_ERROR;
  }

  uint64_t ticket = sequencer->next_in++;
  GCU_Sequencer_Slot * slot = gcu_sequencer_slot(sequencer, ticket);
  slot->payload = payload;
  slot->status = 0;
  slot->used = true;
  slot->done = false;

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  *ticket_out = ticket;
  return GCU_SEQUENCER_OK;
}

GCU_Sequencer_Result gcu_sequencer_submit(
  GCU_Sequencer * sequencer, void * payload, uint64_t * ticket_out) {
  return gcu_sequencer_submit_internal(sequencer, payload, ticket_out, false);
}

GCU_Sequencer_Result gcu_sequencer_submit_wait(
  GCU_Sequencer * sequencer, void * payload, uint64_t * ticket_out) {
  return gcu_sequencer_submit_internal(sequencer, payload, ticket_out, true);
}

GCU_Sequencer_Result gcu_sequencer_complete(
  GCU_Sequencer * sequencer, uint64_t ticket, int status) {
  if (!sequencer) {
    return GCU_SEQUENCER_ERROR;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);

  if (sequencer->shutting_down) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_SHUTDOWN;
  }

  // A ticket outside the outstanding window was never issued, or has already
  // been collected.  Either way there is nothing to record it against.
  if (ticket < sequencer->next_out || ticket >= sequencer->next_in) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_ERROR;
  }

  GCU_Sequencer_Slot * slot = gcu_sequencer_slot(sequencer, ticket);
  if (!slot->used || slot->done) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_ERROR;
  }

  slot->status = status;
  slot->done = true;

  // If this is the item the collectors are waiting for, wake all of them.
  // Only one can take it; the rest re-test, find the head has moved on, and
  // wait again for theirs.
  if (ticket == sequencer->next_out) {
    gcu_sequencer_wake_all(&sequencer->ready, sequencer->result_waiters);
  }

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  return GCU_SEQUENCER_OK;
}

/**
 * Take the head item, which the caller has established is ready.
 *
 * Must be called with the mutex held.
 */
static void gcu_sequencer_take(
  GCU_Sequencer * sequencer, void ** payload_out, int * status_out) {
  GCU_Sequencer_Slot * slot = gcu_sequencer_slot(sequencer, sequencer->next_out);

  if (payload_out) {
    *payload_out = slot->payload;
  }
  if (status_out) {
    *status_out = slot->status;
  }

  slot->payload = NULL;
  slot->used = false;
  slot->done = false;
  ++sequencer->next_out;

  // A slot has freed.  Wake every blocked producer:  only one can take the
  // slot, and the others go back to waiting.
  gcu_sequencer_wake_all(&sequencer->space, sequencer->space_waiters);
}

GCU_Sequencer_Result gcu_sequencer_next(
  GCU_Sequencer * sequencer, void ** payload_out, int * status_out) {
  if (!sequencer) {
    return GCU_SEQUENCER_ERROR;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);

  if (sequencer->shutting_down) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_SHUTDOWN;
  }

  while (!gcu_sequencer_ready(sequencer)) {
    // Nothing is outstanding, so no completion can ever make the head ready.
    // Waiting here would be waiting for a submission that this call has no
    // reason to expect.
    if (gcu_sequencer_outstanding(sequencer) == 0) {
      GCU_MUTEX_UNLOCK(sequencer->mutex);
      return GCU_SEQUENCER_EMPTY;
    }

    ++sequencer->result_waiters;
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    gcu_semaphore_wait(&sequencer->ready);
    GCU_MUTEX_LOCK(sequencer->mutex);
    --sequencer->result_waiters;

    // Released by teardown rather than by a result arriving.
    if (sequencer->shutting_down) {
      GCU_MUTEX_UNLOCK(sequencer->mutex);
      return GCU_SEQUENCER_SHUTDOWN;
    }
  }

  gcu_sequencer_take(sequencer, payload_out, status_out);

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  return GCU_SEQUENCER_OK;
}

GCU_Sequencer_Result gcu_sequencer_try_next(
  GCU_Sequencer * sequencer, void ** payload_out, int * status_out) {
  if (!sequencer) {
    return GCU_SEQUENCER_ERROR;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);

  GCU_Sequencer_Result result = GCU_SEQUENCER_OK;

  if (sequencer->shutting_down) {
    result = GCU_SEQUENCER_SHUTDOWN;
  }
  else if (gcu_sequencer_ready(sequencer)) {
    gcu_sequencer_take(sequencer, payload_out, status_out);
  }
  else if (gcu_sequencer_outstanding(sequencer) == 0) {
    result = GCU_SEQUENCER_EMPTY;
  }
  else {
    result = GCU_SEQUENCER_NOT_READY;
  }

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  return result;
}

GCU_Sequencer_Result gcu_sequencer_reset(GCU_Sequencer * sequencer) {
  if (!sequencer) {
    return GCU_SEQUENCER_ERROR;
  }

  GCU_MUTEX_LOCK(sequencer->mutex);

  if (sequencer->shutting_down) {
    GCU_MUTEX_UNLOCK(sequencer->mutex);
    return GCU_SEQUENCER_SHUTDOWN;
  }

  memset(sequencer->ring, 0,
    sequencer->ring_size * sizeof(GCU_Sequencer_Slot));
  sequencer->next_in = 0;
  sequencer->next_out = 0;

  // Everything is outstanding no longer, so every blocked thread has a new
  // answer:  producers have room, and collectors have an empty sequencer.
  gcu_sequencer_wake_all(&sequencer->space, sequencer->space_waiters);
  gcu_sequencer_wake_all(&sequencer->ready, sequencer->result_waiters);

  GCU_MUTEX_UNLOCK(sequencer->mutex);

  return GCU_SEQUENCER_OK;
}

size_t gcu_sequencer_count_outstanding(const GCU_Sequencer * sequencer) {
  if (!sequencer) {
    return 0;
  }

  // The mutex is an implementation detail, not logical state, so taking it
  // through a const pointer is honest.
  GCU_Sequencer * mutable_sequencer = (GCU_Sequencer *)sequencer;

  GCU_MUTEX_LOCK(mutable_sequencer->mutex);
  size_t count = gcu_sequencer_outstanding(mutable_sequencer);
  GCU_MUTEX_UNLOCK(mutable_sequencer->mutex);

  return count;
}

size_t gcu_sequencer_capacity(const GCU_Sequencer * sequencer) {
  return sequencer ? sequencer->capacity : 0;
}

bool gcu_sequencer_is_ready(const GCU_Sequencer * sequencer) {
  if (!sequencer) {
    return false;
  }

  GCU_Sequencer * mutable_sequencer = (GCU_Sequencer *)sequencer;

  GCU_MUTEX_LOCK(mutable_sequencer->mutex);
  bool ready = gcu_sequencer_ready(mutable_sequencer);
  GCU_MUTEX_UNLOCK(mutable_sequencer->mutex);

  return ready;
}

bool gcu_sequencer_is_shutting_down(const GCU_Sequencer * sequencer) {
  if (!sequencer) {
    return false;
  }

  GCU_Sequencer * mutable_sequencer = (GCU_Sequencer *)sequencer;

  GCU_MUTEX_LOCK(mutable_sequencer->mutex);
  bool shutting_down = mutable_sequencer->shutting_down;
  GCU_MUTEX_UNLOCK(mutable_sequencer->mutex);

  return shutting_down;
}
