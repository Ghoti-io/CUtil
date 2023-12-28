/**
 * @file
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

bool capture = true;

#define GHOTIIO_CUTIL_DEBUG_DO_NOT_REDECLARE_MEMORY_FUNCTIONS
#include "cutil/debug.h"

void * gcu_malloc(size_t size, const char * file, size_t line) {
  void * result = malloc(size);
  if (capture) {
    fprintf(stderr, "malloc  | %p:%zu | %s(%zu)\n", result, size, file, line);
  }
  return result;
}

void * gcu_calloc(size_t nitems, size_t size, const char * file, size_t line) {
  void * result = calloc(nitems, size);
  if (capture) {
    fprintf(stderr, "calloc  | %p:%zu:%zu | %s(%zu)\n", result, nitems, size, file, line);
  }
  return result;
}

void * gcu_realloc(void * pointer, size_t size, const char * file, size_t line) {
  void * result = realloc(pointer, size);
  if (capture) {
    fprintf(stderr, "realloc | %p:%zu -> %p | %s(%zu)\n", pointer, size, result, file, line);
  }
  return result;
}

void gcu_free(void * pointer, const char * file, size_t line) {
  if (capture) {
    fprintf(stderr, "free    | %p | %s(%zu)\n", pointer, file, line);
  }
  free(pointer);
}

void gcu_mem_start(void) {
  capture = true;
}

void gcu_mem_stop(void) {
  capture = false;
}

