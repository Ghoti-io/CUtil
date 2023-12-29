GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _create)(size_t count) {
  // Malloc Zeroed-out memory.
  GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * vector = calloc(1, sizeof(GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH)));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _destroy)(GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * vector) {
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

static bool GHOTIIO_CUTIL_CONCAT2(grow_vector, BITDEPTH)(GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * newTable = GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _create)(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union))));
  if (newMem) {
    // Zero out the new memory.
    memset((GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union) *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _append)(GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * vector, GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union) value) {
  if ((vector->count >= vector->capacity) && !GHOTIIO_CUTIL_CONCAT2(grow_vector, BITDEPTH)(vector, vector->count < 32
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

size_t GHOTIIO_CUTIL_CONCAT3(gcu_vector, BITDEPTH, _count)(GHOTIIO_CUTIL_CONCAT2(GCU_Vector, BITDEPTH) * vector) {
  return vector->count;
}

