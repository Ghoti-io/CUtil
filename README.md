# Ghoti.io CUtil
This is a collection of Cross-Platform libraries written in C.  It is a work in progress.

## Libraries

### Fixed-size Float

Provides 2 types: `GCU_float64_t` and `GCU_float32_t` which are generated during the build process to be correct for the system on which it is being compiled.

### Type Unions

Provides type unions based on bit size for use in other parts of this library.  Names are `GCU_Type64_Union`, `GCU_Type32_Union`, `GCU_Type16_Union`, and `GCU_Type8_Union`.  Union contains all basic types that will fit into that bit size.  Pointers, for example, only exist in the `64`-bit union.  The programmer is responsible for the memory management of the pointed-to data.

### Memory Library

Provides functions `gcu_malloc()`, `gcu_calloc()`, `gcu_realloc()`, and `gcu_free()` which are used by all other parts of the library.  Calling `gcu_mem_start()` and `gcu_mem_stop()` will cause all calls to the afore-mentioned memory functions to be logged to `stderr`, including the calling location and the memory locations involved, making memory errors easy to track down.

### String

Provides several functions which will calculate a hash on a set of bytes using the **Murmur3** algorithm.

### Hash Table

Provides hash tables that hold `8`, `16`, `32`, and `64`-bit values.

The programmer must supply a hash value which will uniquely identify the object to be stored/retrieved, but the `string.h` library provides a good and fast helper algorithm, **Murmur3**, to make this easy.

The hash table will also have a mutex, but it is the programmer's responsibility to use it when appropriate.

The programmer may provide a `cleanup` function which will be called when the hash table is destroyed.

### Vector

Provides a generalized vector structure that, similar to the hash tables, will hold `8`, `16`, `32`, and `64`-bit values.

The vector will also have a mutex, but it is the programmer's responsibility to use it when appropriate.

The programmer may provide a `cleanup` function which will be called when the vector is destroyed.

### Thread

Provides a thread abstraction layer to better manage threads and information about the threads.

### Mutex

Provides a mutex abstraction for use as a low-level synchronization tool.

### Semaphore

Provides a counting semaphore implementation with a user-configurable limit.  A counting semaphore with a limit of 1 will be, in effect, a binary semaphore.

## Tests

All librarys contain a corresponding test written in C++ (demonstrating that the library can be used in C++ as well as C) using the Google Test (`gtest`) framework.

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
