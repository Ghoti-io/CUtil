/**
 * @file
 */

#include <stdlib.h>
#include <string.h>
#include "cutil/vector.h"

#define GROWTH_FACTOR 1.3

GCU_Vector * gcu_vector_create(size_t count) {
  // Malloc Zeroed-out memory.
  GCU_Vector * vector = calloc(1, sizeof(GCU_Vector));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GCU_Type_Union));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void gcu_vector_destroy(GCU_Vector * vector) {
  // Verify that the pointer actually points to something.
  if (vector) {
    // Clean up the data table if needed.
    if (vector->data) {
      free(vector->data);
      vector->data = 0;
    }
    free(vector);
  }
}

static bool grow_vector(GCU_Vector * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GCU_Vector * newTable = gcu_vector_create(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GCU_Type_Union)));
  if (newMem) {
    // Zero out the new memory.
    memset((GCU_Type_Union *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GCU_Type_Union));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool gcu_vector_append(GCU_Vector * vector, GCU_Type_Union value) {
  if ((vector->count >= vector->capacity) && !grow_vector(vector, vector->count < 32
      ? 32
      : vector->count < 1024
        ? vector->count * 2
        : vector->count * GROWTH_FACTOR)) {
    return false;
  }
  vector->data[vector->count] = value;
  ++vector->count;
  return true;
}

size_t gcu_vector_count(GCU_Vector * vector) {
  return vector->count;
}

