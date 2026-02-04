#define TEMPLATE_GCU_VECTOR         GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH)
#define TEMPLATE_GCU_TYPE_UNION     GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)
#define TEMPLATE_GCU_VECTOR_CREATE  GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _create)
#define TEMPLATE_GCU_VECTOR_CREATE_IN_PLACE GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _create_in_place)
#define TEMPLATE_GCU_VECTOR_DESTROY GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _destroy)
#define TEMPLATE_GCU_VECTOR_DESTROY_IN_PLACE GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _destroy_in_place)
#define TEMPLATE_GCU_VECTOR_APPEND  GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _append)
#define TEMPLATE_GCU_VECTOR_COUNT   GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _count)
#define TEMPLATE_GCU_VECTOR_RESERVE GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _reserve)

TEMPLATE_GCU_VECTOR * TEMPLATE_GCU_VECTOR_CREATE(size_t count) {
  // Malloc Zeroed-out memory.
  TEMPLATE_GCU_VECTOR * vector = gcu_calloc(1, sizeof(TEMPLATE_GCU_VECTOR));

  // If the allocation failed, return null.
  if (!vector) {
    return 0;
  }

  if (!TEMPLATE_GCU_VECTOR_CREATE_IN_PLACE(vector, count)) {
    gcu_free(vector);
    return 0;
  }

  return vector;
}

bool TEMPLATE_GCU_VECTOR_CREATE_IN_PLACE(TEMPLATE_GCU_VECTOR * vector, size_t count) {
  *vector = (TEMPLATE_GCU_VECTOR) {
    .count = 0,
    .capacity = 0,
    .data = 0,
    .cleanup = 0,
  };

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
    return false;
  }

  return true;
}

void TEMPLATE_GCU_VECTOR_DESTROY(TEMPLATE_GCU_VECTOR * vector) {
  if (vector) {
    TEMPLATE_GCU_VECTOR_DESTROY_IN_PLACE(vector);
    gcu_free(vector);
  }
}

void TEMPLATE_GCU_VECTOR_DESTROY_IN_PLACE(TEMPLATE_GCU_VECTOR * vector) {
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
  }
}

bool TEMPLATE_GCU_VECTOR_APPEND(TEMPLATE_GCU_VECTOR * vector, TEMPLATE_GCU_TYPE_UNION value) {
  if ((vector->count >= vector->capacity) && !TEMPLATE_GCU_VECTOR_RESERVE(vector, vector->count < 32
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

bool TEMPLATE_GCU_VECTOR_RESERVE(TEMPLATE_GCU_VECTOR * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  // Verify that the requested size is larger than the current capacity.
  if (size < vector->capacity) {
    return true;
  }

  // Attempt to allocate more memory;
  if (!vector->data) {
    vector->data = gcu_calloc(size, sizeof (TEMPLATE_GCU_TYPE_UNION));
    if (vector->data) {
      vector->capacity = size;
      return true;
    }
    return false;
  }
  void * newMem = gcu_realloc(vector->data, size * (sizeof (TEMPLATE_GCU_TYPE_UNION)));
  if (newMem) {
    // Zero out the new memory.
#ifdef DEBUG
    memset((TEMPLATE_GCU_TYPE_UNION *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(TEMPLATE_GCU_TYPE_UNION));
#endif

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

#undef TEMPLATE_GCU_VECTOR
#undef TEMPLATE_GCU_TYPE_UNION
#undef TEMPLATE_GCU_VECTOR_CREATE
#undef TEMPLATE_GCU_VECTOR_CREATE_IN_PLACE
#undef TEMPLATE_GCU_VECTOR_DESTROY
#undef TEMPLATE_GCU_VECTOR_DESTROY_IN_PLACE
#undef TEMPLATE_GCU_VECTOR_APPEND
#undef TEMPLATE_GCU_VECTOR_COUNT
#undef TEMPLATE_GCU_VECTOR_RESERVE

