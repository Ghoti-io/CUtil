# Memory counting

## 1. What it is

`gcu_malloc`, `gcu_calloc`, `gcu_realloc` and `gcu_free` are thin wrappers
over the platform allocator that also keep two running totals, readable
through `gcu_get_alloc_count()` and `gcu_get_free_count()`.

The totals exist so that a program can assert, at the end of a run, that it
released everything it took.  That assertion is the whole point, and
everything below is about the ways it can quietly stop meaning anything.

## 2. They count blocks, not calls

This is the rule the whole facility rests on:

> `gcu_get_alloc_count()` minus `gcu_get_free_count()` is the number of blocks
> still outstanding.

Which means the counting follows what happened to the heap, not what the
caller wrote:

| call | counted as |
|---|---|
| `gcu_malloc(n)` / `gcu_calloc(n, m)` | one allocation |
| `gcu_realloc(NULL, n)` | one allocation - it allocates a block |
| `gcu_realloc(p, n)`, `p` non-`NULL` | nothing - no block begins or ends |
| `gcu_free(p)`, `p` non-`NULL` | one free |
| `gcu_free(NULL)` | nothing - it releases nothing |

It did not always work this way, and the way it failed is worth keeping
because the failure was invisible.  `gcu_realloc()` used to count nothing even
when growing from `NULL`, while `gcu_free()` counted unconditionally.  That
balances for a block that is `malloc`ed, grown and freed.  It does not balance
for the way this library's own containers allocate: a `GCU_Array` takes its
storage only through `gcu_allocator_realloc()` against a pointer that starts
`NULL`, so its buffer was released having never been counted.

The obvious symptom was a false leak report - a correct, fully cleaned-up
array left the free count *above* the allocation count.  The dangerous symptom
was the opposite one.  A net that runs negative does not merely cry wolf; it
cancels a real leak, one block for one block.  One leaked block beside one
correct array balanced exactly, and the assertion written to catch that leak
passed.  **An assertion defeated by the correct use of an API.**

The general form, which outlives this particular bug: when a counter
disagrees with reality, ask what unit it counts before fixing the arithmetic,
and then check the sign.  A count that runs *under* is far worse than one that
runs over, because over merely raises a false alarm while under silently
absolves.

## 3. `NULL` means failure and nothing else

Two of the wrappers depart from the C library, both to keep that promise:

- `gcu_realloc(NULL, n)` allocates, as `realloc()` does.
- `gcu_realloc(p, 0)` does **not** release the block.  `realloc(p, 0)` on
  glibc frees it and hands back `NULL`, which the caller cannot tell apart
  from a failure that left the block alive - so the caller either leaks it or
  frees it twice, and the release goes uncounted either way.  The size is
  treated as one instead.  `gcu_free()` is the only way to release anything.

`gcu_allocator_default()` keeps the same rule for all three of its entry
points, for the same reason.

## 4. The counters do not start at zero

cutil's thread module allocates a small number of blocks in a library
constructor, before `main()` runs, and releases them in a destructor, after
`main()` returns.  Nothing leaks - valgrind is clean - but any assertion
written *inside* the program sees those allocations and not the matching
frees:

```c
int main(void) {
  // alloc=3 free=0 here, in every program that links cutil
}
```

So compare **deltas**, or call `gcu_memory_reset_counts()` first.  Never
compare the two absolute totals:

```c
size_t alloc = gcu_get_alloc_count();
size_t freed = gcu_get_free_count();
... the code under test ...
ASSERT_EQ(gcu_get_alloc_count() - alloc, gcu_get_free_count() - freed);
```

The exact number is an implementation detail and may change.  Code that
depends on it being three is already wrong.

## 5. The counting is on whether or not you asked for debugging

`GHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG` adds the `stderr` tracing.  It does not
switch the counting on: the inline versions in `memory.h`, which the library
and every consumer are built with, increment the same totals.

This matters because a `Makefile` with that define sitting commented out reads
as though the counters are inert.  They are not, and the
`ASSERT_EQ(gcu_get_alloc_count(), gcu_get_free_count())` assertions scattered
through the suites are live.

It also means the rules in section 2 are implemented **twice** - the debug
functions in `memory.c` and the inline versions in `memory.h`.
`test/test-memory.cpp` pins the first and `test/test-memory-inline.cpp` the
second.  Testing only the debug path would leave the shipped behaviour
unchecked, which was the situation until the inline file was written.

## 6. Finding the call site of an imbalance

Do not write a probe for this.  Compile the code in question with
`-DGHOTIIO_CUTIL_ENABLE_MEMORY_DEBUG` and every call is traced to `stderr`
with its file and line:

```
malloc  | 4 | 0x55e194109a40:16 | src/thing.c(15)
free    | 1 | 0x55e194109a40 | src/thing.c(16)
free    | 1 | (nil) | src/thing.c(17)
realloc | (nil):16 -> 0x55e194109a40 | src/thing.c(8)
```

A `gcu_free(NULL)` shows twice over: the pointer prints as `(nil)`, and the
count ahead of it does not advance.  A `gcu_realloc()` from `NULL` likewise
shows `(nil)` as its old pointer.  `gcu_mem_stop()` and `gcu_mem_start()`
bracket a region whose trace you do not want.

The define has to be in effect where the *call* is compiled, not where the
library was, because these are macros.  A build that instruments only `src/`
and not the tests will miss any allocation the tests perform directly - and a
probe that sees nothing looks exactly like a program that does nothing wrong.
Whatever the mechanism, confirm it fires on a case you know is there before
believing a zero.

## 7. What counts, and what does not

The counters only move for memory taken through `gcu_malloc` and its
siblings.  In particular `gcu_allocator_default()` is backed by the C library
directly and counts **nothing**.

A library that wants its container use counted has to supply a
`GCU_Allocator` whose four functions call the cutil wrappers.  That is
required anyway for a different reason: the default allocator hands out
`malloc()` blocks, so a caller who releases them with `gcu_free()` has a count
mismatch at best and two allocators arguing at worst.

## 8. Known wrong

A *failed* allocation is still counted.  `gcu_malloc()` increments before it
knows whether it got anything, so an out-of-memory return leaves the totals
permanently one apart in the direction that reads as a leak.

It is left alone rather than fixed because it cannot be pinned by a test in
this suite: ASan makes an overflowing `calloc()` fatal and valgrind rejects an
absurd `malloc()` size, so neither gate will let a test provoke the case.  A
behaviour change that no test can hold is worse than a documented gap.
