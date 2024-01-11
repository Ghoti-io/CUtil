#define TEMPLATE_GROW_VECTOR        GHOTIIO_CUTIL_CONCAT2(grow_vector, BITDEPTH)
#define TEMPLATE_GCU_VECTOR         GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH)
#define TEMPLATE_GCU_TYPE_UNION     GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)
#define TEMPLATE_GCU_VECTOR_CREATE  GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _create)
#define TEMPLATE_GCU_VECTOR_DESTROY GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _destroy)
#define TEMPLATE_GCU_VECTOR_APPEND  GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _append)
#define TEMPLATE_GCU_VECTOR_COUNT   GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _count)

TEMPLATE_GCU_VECTOR * TEMPLATE_GCU_VECTOR_CREATE(size_t count) {
  // Malloc Zeroed-out memory.
  TEMPLATE_GCU_VECTOR * vector = gcu_calloc(1, sizeof(TEMPLATE_GCU_VECTOR));

  // If the allocation failed, return null.
  if (!vector) {
    return 0;
  }

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = gcu_calloc(count, sizeof(TEMPLATE_GCU_TYPE_UNION));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  // Allocate the mutex.
  bool failure = GCU_MUTEX_CREATE(vector->mutex);

  // If the allocation failed, clean up and return null.
  if (failure) {
    if (vector->data) {
      gcu_free(vector->data);
    }
    gcu_free(vector);
    return 0;
  }

  return vector;
}

void TEMPLATE_GCU_VECTOR_DESTROY(TEMPLATE_GCU_VECTOR * vector) {
  // Verify that the pointer actually points to something.
  if (vector) {
    // Call the `cleanup` function, if it exists.
    if (vector->cleanup) {
      vector->cleanup(vector);
    }

    // Clean up the data table if needed.
    if (vector->data) {
      gcu_free(vector->data);
      vector->data = 0;
    }

    GCU_MUTEX_DESTROY(vector->mutex);
    gcu_free(vector);
  }
}

static bool TEMPLATE_GROW_VECTOR(TEMPLATE_GCU_VECTOR * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  TEMPLATE_GCU_VECTOR * newTable = TEMPLATE_GCU_VECTOR_CREATE(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = gcu_realloc(vector->data, size * (sizeof (TEMPLATE_GCU_TYPE_UNION)));
  if (newMem) {
    // Zero out the new memory.
    memset((TEMPLATE_GCU_TYPE_UNION *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(TEMPLATE_GCU_TYPE_UNION));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool TEMPLATE_GCU_VECTOR_APPEND(TEMPLATE_GCU_VECTOR * vector, TEMPLATE_GCU_TYPE_UNION value) {
  if ((vector->count >= vector->capacity) && !TEMPLATE_GROW_VECTOR(vector, vector->count < 32
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

size_t TEMPLATE_GCU_VECTOR_COUNT(TEMPLATE_GCU_VECTOR * vector) {
  return vector->count;
}

#undef TEMPLATE_GROW_VECTOR
#undef TEMPLATE_GCU_VECTOR
#undef TEMPLATE_GCU_TYPE_UNION
#undef TEMPLATE_GCU_VECTOR_CREATE
#undef TEMPLATE_GCU_VECTOR_DESTROY
#undef TEMPLATE_GCU_VECTOR_APPEND
#undef TEMPLATE_GCU_VECTOR_COUNT

