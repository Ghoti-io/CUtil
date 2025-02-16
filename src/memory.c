/**
 */

#include <stdbool.h>
#include <stdio.h>

static bool capture = true;

size_t gcu_memory_alloc_count = 0;
size_t gcu_memory_free_count = 0;

#include <cutil/memory.h>

/// @cond HIDDEN_SYMBOLS
void * gcu_malloc_debug(size_t size, const char * file, size_t line) {
  ++gcu_memory_alloc_count;
  void * result = malloc(size);
  if (capture) {
    fprintf(stderr, "malloc  | %zd | %p:%zu | %s(%zu)\n", gcu_memory_alloc_count, result, size, file, line);
  }
  return result;
}

void * gcu_calloc_debug(size_t nitems, size_t size, const char * file, size_t line) {
  ++gcu_memory_alloc_count;
  void * result = calloc(nitems, size);
  if (capture) {
    fprintf(stderr, "calloc  | %zd | %p:%zu:%zu | %s(%zu)\n", gcu_memory_alloc_count, result, nitems, size, file, line);
  }
  return result;
}

void * gcu_realloc_debug(void * pointer, size_t size, const char * file, size_t line) {
  // Note: This is a bit of a hack.  We're using sprintf() to convert a pointer
  // to a string, simply because some compilers were complaining about a
  // possible "use after free" error when we tried to use the pointer directly
  // in the fprintf() call (which was after the realloc() call).
  char buffer[32];
  if (capture) {
    snprintf(buffer, 32, "%p", pointer);
  }

  void * result = realloc(pointer, size);
  if (capture) {
    fprintf(stderr, "realloc | %s:%zu -> %p | %s(%zu)\n", buffer, size, result, file, line);
  }
  return result;
}

void gcu_free_debug(void * pointer, const char * file, size_t line) {
  ++gcu_memory_free_count;
  if (capture) {
    fprintf(stderr, "free    | %zd | %p | %s(%zu)\n", gcu_memory_free_count, pointer, file, line);
  }
  free(pointer);
}
/// @endcond

void gcu_mem_start(void) {
  capture = true;
}

void gcu_mem_stop(void) {
  capture = false;
}

size_t gcu_get_alloc_count(void) {
  return gcu_memory_alloc_count;
}

size_t gcu_get_free_count(void) {
  return gcu_memory_free_count;
}

void gcu_memory_reset_counts(void) {
  gcu_memory_alloc_count = 0;
  gcu_memory_free_count = 0;
}