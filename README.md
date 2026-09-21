# Ghoti.io CUtil
This is a collection of Cross-Platform libraries written in C.  It is a work in progress.

## Libraries

### Fixed-size Float

Provides 2 types: `GCU_float64_t` and `GCU_float32_t` which are generated during the build process to be correct for the system on which it is being compiled.

### Type Unions

Provides type unions based on bit size for use in other parts of this library.  Names are `GCU_Type64_Union`, `GCU_Type32_Union`, `GCU_Type16_Union`, and `GCU_Type8_Union`.  Union contains all basic types that will fit into that bit size.  Pointers, for example, only exist in the `64`-bit union.  The programmer is responsible for the memory management of the pointed-to data.

### Allocator

Provides `GCU_Allocator`, a small vtable (a user-defined `ctx` pointer plus `malloc_fn`, `calloc_fn`, `realloc_fn`, and `free_fn`) that lets a caller supply its own allocation strategy -- an arena, a pool, or a tracking allocator -- to any part of this library that accepts one.

`gcu_allocator_default()` returns the standard allocator, which forwards to the Memory Library described below.  The `gcu_allocator_malloc()`, `gcu_allocator_calloc()`, `gcu_allocator_realloc()`, and `gcu_allocator_free()` helpers dispatch through a given allocator so that calling code does not need to touch the function pointers directly.

### Memory Library

Provides functions `gcu_malloc()`, `gcu_calloc()`, `gcu_realloc()`, and `gcu_free()` which are used by all other parts of the library.  Calling `gcu_mem_start()` and `gcu_mem_stop()` will cause all calls to the afore-mentioned memory functions to be logged to `stderr`, including the calling location and the memory locations involved, making memory errors easy to track down.

### String

Provides several functions which will calculate a hash on a set of bytes using the **Murmur3** algorithm.

### Array

Provides `GCU_Array`, a generalized growable array.  Unlike the Vector (below), it is byte-oriented: the element size is fixed when the array is created, so an array may hold arbitrary structures rather than only values that fit into a fixed-width union.

The array takes a `GCU_Allocator` (see above), and may be created either on the heap (`gcu_array_create()` / `gcu_array_destroy()`) or in memory the caller already owns (`gcu_array_create_in_place()` / `gcu_array_destroy_in_place()`).

Capacity is managed with `gcu_array_reserve()`, `gcu_array_resize()`, and `gcu_array_shrink_to_fit()`.  Elements may be added by copy with `gcu_array_append()` and `gcu_array_append_n()`, or constructed in place by taking a writable slot from `gcu_array_emplace()` and `gcu_array_emplace_n()`, which avoids a copy.  Elements are read with `gcu_array_at()` and `gcu_array_back()`, and removed with `gcu_array_pop()`, `gcu_array_remove_at()` (order-preserving), and `gcu_array_swap_remove()` (constant-time, does not preserve order).  `gcu_array_steal()` hands the backing buffer to the caller and leaves the array empty.

The programmer may provide a `cleanup` function which will be called when the array is destroyed, and a `supplementary_data` pointer for the cleanup function's use.

Note that, unlike the Hash Table and Vector, the array does *not* carry a mutex.  Callers needing one should use the Mutex library directly.

### Hash Table

Provides hash tables that hold `8`, `16`, `32`, and `64`-bit values.

The programmer must supply a hash value which will uniquely identify the object to be stored/retrieved, but the `string.h` library provides a good and fast helper algorithm, **Murmur3**, to make this easy.

The hash table will also have a mutex, but it is the programmer's responsibility to use it when appropriate.

The programmer may provide a `cleanup` function which will be called when the hash table is destroyed.

### Vector

Provides a generalized vector structure that, similar to the hash tables, will hold `8`, `16`, `32`, and `64`-bit values.

The vector will also have a mutex, but it is the programmer's responsibility to use it when appropriate.

The programmer may provide a `cleanup` function which will be called when the vector is destroyed.

For elements that do not fit into one of those fixed-width unions -- arbitrary structures, for example -- see the Array library above, which is byte-oriented and takes a caller-supplied allocator.

### Safe Math

Provides header-only, overflow-checked integer arithmetic.  Each function returns `true` on success and writes through its `result` pointer, or returns `false` if the operation would overflow, leaving `result` untouched.

Available for `size_t` are `gcu_safe_add_size()`, `gcu_safe_sub_size()`, `gcu_safe_mul_size()`, `gcu_safe_add3_size()`, and `gcu_safe_mul_add_size()` -- the last of which covers the common allocation-sizing idiom of `(count * element_size) + header`.  `gcu_safe_add_u64()`, `gcu_safe_mul_u64()`, `gcu_safe_add_u32()`, and `gcu_safe_mul_u32()` provide the same for fixed-width unsigned types.

Where the compiler provides them, these use the `__builtin_*_overflow()` intrinsics and `GCU_HAS_BUILTIN_OVERFLOW` is defined; otherwise a portable fallback using division and subtraction checks is compiled instead.  Both paths are tested.

### Random

Provides the Mersenne Twister pseudo-random number generator in both 32-bit (`gcu_random_mt32_init()` / `gcu_random_mt32_next()`) and 64-bit (`gcu_random_mt64_init()` / `gcu_random_mt64_next()`) forms.

The generator state is held in a caller-owned `GCU_Random_MT32_State` or `GCU_Random_MT64_State` structure rather than in a global, so that separate streams do not interfere with one another and a seeded sequence is reproducible.

### Thread Pool

Provides `GCU_Pool`, a fixed set of worker threads drawing tasks from a shared FIFO queue.  Built on the Thread, Mutex and Semaphore libraries below; it needs no condition variable, because a counting semaphore expresses a task queue directly.

A task is a function returning `0` for success or any non-zero status for failure.  `gcu_pool_enqueue()` hands one to the pool, `gcu_pool_enqueue_cb()` attaches a completion callback, and `gcu_pool_wait()` blocks until the pool is idle and reports the first non-zero status any task returned.  Reading that status does not consume it, so several threads may wait and all see the same answer.

`gcu_pool_destroy()` **drains**: it runs everything already queued before stopping the workers, so nothing successfully enqueued is silently lost.  `gcu_pool_abandon()` discards the queue instead.  Draining is the default because forgetting it loses completed work and reports no error, while forgetting to abandon only costs some waiting.

A `thread_count` of `0` selects inline mode, in which tasks run on the calling thread and no threads are created -- useful for deterministic tests.  `GCU_POOL_THREADS_AUTO` selects one worker per logical processor.  A count of `1` means one worker thread, not inline.

The queue is unbounded by default.  Setting `max_queued` makes `gcu_pool_enqueue()` fail once the queue is full and `gcu_pool_enqueue_wait()` block until a slot frees.

Workers are named `<prefix>-<index>` from a caller-supplied `name_prefix`, which shows up in a debugger when several pools are running at once.

The design and the reasoning behind each decision are in `documentation/thread-pool.md`.

### File

Provides whole-file reading and atomic whole-file replacement.  Built on the Path library below; it adds nothing of its own about path syntax.

`gcu_file_read()` reads a file into an allocator-owned buffer.  It reads in **chunks rather than sizing the file first**, so it works on inputs that report no size at all -- pipes, character devices, and everything under `/proc`, where a seek-and-tell implementation silently returns an empty buffer.  The buffer carries a NUL one byte past `out_len` that is not counted in it, so a text caller can use the result as a C string without copying and a binary caller can ignore it.  `max_bytes` is a promise:  a file over the limit gives `GCU_FILE_ERR_LIMIT` and nothing is allocated, never a truncation.

`GCU_File_Temp` is a temporary file, created and opened in one step that fails rather than following a symbolic link somebody else put there, and readable only by its owner.  It is disposed of by exactly one of `gcu_file_temp_commit()` (move it into place) or `gcu_file_temp_abort()` (delete it).  Both leave the handle zeroed and `abort` accepts a zeroed handle, so `abort` may sit on an unconditional cleanup path without tracking whether `commit` already ran.

`gcu_file_write_atomic()` is the whole sequence for content already in memory.  The temporary goes in the **destination's own directory**, because a rename across filesystems is a copy and a copy is not atomic.

`GCU_FILE_SYNC_FULL` is the zero value, so a caller who does not think about it gets durability:  the content is committed before the rename, and a failure there is reported.  The directory entry is committed afterwards on a best-effort basis, because several filesystems refuse the request and failing an otherwise complete replacement over it would be worse.  `GCU_FILE_SYNC_NONE` keeps the atomicity and gives up only the durability.

The design and the reasoning behind each decision are in `documentation/file.md`.

### Path

Provides path manipulation, split into a lexical half that never touches the filesystem and an environment half that asks the operating system.

The lexical half -- `gcu_path_join()`, `gcu_path_normalize()`, `gcu_path_dirname()`, `gcu_path_basename()`, `gcu_path_extension()`, `gcu_path_is_absolute()`, `gcu_path_root_length()`, `gcu_path_relative_to()`, `gcu_path_to_native()`, `gcu_path_to_posix()` -- takes a `GCU_Path_Flavor` rather than reading `#ifdef _WIN32`.  That is what makes the Windows rules testable:  drive letters, UNC shares and the difference between *rooted* and *absolute* are ordinary functions, and every Windows case in `test/test-path.cpp` runs on Linux.

`C:\x` is absolute; `C:x` and `\x` are **not**, though all three have a root.  `gcu_path_root_length()` and `gcu_path_is_absolute()` are separate questions with different answers, and `..` may climb above `C:` but not above `C:\`.

The environment half -- `gcu_path_cwd()`, `gcu_path_home()`, `gcu_path_config_dir()`, `gcu_path_data_dir()`, `gcu_path_cache_dir()`, `gcu_path_temp_dir()`, `gcu_path_absolute()`, `gcu_path_canonicalize()` -- allocates, because the length of an answer from the operating system is not knowable before asking.  Prefer the purpose-specific directories to `gcu_path_home()`:  code that appends `/.myapp` to a home directory is correct on Linux and wrong on Windows and macOS.

`gcu_path_absolute()` resolves a path lexically and `gcu_path_canonicalize()` resolves it through the filesystem, following symbolic links.  They disagree whenever a link is involved, and a containment check written on the lexical one is not a containment check.

Lexical calls write into a caller-supplied buffer and never truncate:  a shortened path is still a valid path, and it names a different file.  Passing `NULL` with size `0` measures.

There is deliberately no `chdir`.  A process has one working directory shared by every thread, and a library that changes it alters the meaning of every relative path in its host application.  Pass a base directory and join onto it.

The design and the reasoning behind each decision are in `documentation/path.md`.

### Sequencer

Provides `GCU_Sequencer`, a reorder buffer:  items go in, are finished in any order at all, and come back out in the order they went in.

`gcu_sequencer_submit()` stores a `void *` payload and returns a **ticket**.  The work happens wherever the caller likes -- typically on a `GCU_Pool` -- and whoever finishes an item calls `gcu_sequencer_complete()` with its ticket and a status.  `gcu_sequencer_next()` then hands items back strictly in submission order, blocking while the oldest uncollected one is still outstanding.

It pairs with the Thread Pool and does not overlap it:  the pool decides *when* work runs, the sequencer decides *what order results are seen in*.  Parallel block compression is the shape it was drawn from -- compress every block at once, write them out in file order -- but nothing about it is specific to compression.  The sequencer never dereferences a payload and never interprets a status.

`capacity` bounds how many items may be outstanding, which is how a producer is kept from running arbitrarily far ahead of a consumer.  `gcu_sequencer_submit()` reports `GCU_SEQUENCER_FULL` rather than waiting, and is the call to reach for:  space is freed only by `gcu_sequencer_next()`, so a caller that collects its own results and blocks in `gcu_sequencer_submit_wait()` would be waiting for space that only it could free.  The blocking form is correct when a *different* thread collects.

An empty sequencer returns `GCU_SEQUENCER_EMPTY` rather than blocking, since with nothing in flight no completion could ever arrive.  That makes EMPTY a usable end-of-stream signal for a caller that does not count its own items, at the price that a collector which can outrun its producer must treat it as "not yet".

The design and the reasoning behind each decision are in `documentation/sequencer.md`.

### Thread

Provides a thread abstraction layer to better manage threads and information about the threads.

### Mutex

Provides a mutex abstraction for use as a low-level synchronization tool.

### Semaphore

Provides a counting semaphore implementation with a user-configurable limit.  A counting semaphore with a limit of 1 will be, in effect, a binary semaphore.

### Portability Macros

Provides the macros used to keep the rest of the library portable across compilers and platforms.  `GCU_API` and `GCU_API_DATA` expand to the correct export or import decoration (`__declspec(dllexport)`, `__declspec(dllimport)`, or `__attribute__((visibility("default")))`) for the current target, and `GCU_EXTERN` handles `extern "C"` when compiling as C++.

Also provided are `GCU_MAYBE_UNUSED()` and `GCU_DEPRECATED` attribute wrappers, the `GCU_WCHAR_WIDTH` and `GCU_WCHAR_SIGNED` detection macros, and `GCU_INIT_FUNCTION()` / `GCU_CLEANUP_FUNCTION()` for declaring functions that run before `main()` and after it returns.

### Symbol Namespacing and Versioning

Every public symbol in this library is mapped through the `GHOTIIO_CUTIL()` macro, which prefixes it with a build-specific token.  That token is derived from the Makefile's `BRANCH`, and it is the same token that names the shared library, the `.pc` file, and the install directory.  The result is that two projects may embed two different versions of this library in the same program without their symbols colliding.  Building with `make BRANCH=-dev` produces a build with its own identity throughout.

`libver.h` exposes `GCU_VERSION_MAJOR`, `GCU_VERSION_MINOR`, `GCU_VERSION_PATCH`, and `GCU_VERSION_STRING`.  For compile-time comparisons, `GCU_VERSION_NUMBER` packs the current version into a single integer which may be tested against `GCU_MAKE_VERSION(major, minor, patch)`.

## Tests

All libraries contain a corresponding test written in C++ (demonstrating that the library can be used in C++ as well as C) using the Google Test (`gtest`) framework.

## Documentation

All prototypes, typedefs, and defines are documented using Doxygen.  Documentation should be available under the `/docs` folder.

## Compiling

### Linux

```
make
make install
```

### Windows

Still working on this, but will probably focus on the Mingw toolchain.

## License

LGPL-3.0-only. See [COPYING.LESSER](COPYING.LESSER) for the license, and
[COPYING](COPYING) for the GPL text it is written as additional permissions
on top of.

Contributions are not being accepted at this time; see
[CONTRIBUTING.md](CONTRIBUTING.md) for what is useful instead.
