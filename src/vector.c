/**
 * @file
 */

#include <stdlib.h>
#include <string.h>
#include "cutil/vector.h"

#define GROWTH_FACTOR 1.3

GCU_Vector64 * gcu_vector64_create(size_t count) {
  // Malloc Zeroed-out memory.
  GCU_Vector64 * vector = calloc(1, sizeof(GCU_Vector64));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GCU_Type64_Union));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void gcu_vector64_destroy(GCU_Vector64 * vector) {
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

static bool grow_vector64(GCU_Vector64 * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GCU_Vector64 * newTable = gcu_vector64_create(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GCU_Type64_Union)));
  if (newMem) {
    // Zero out the new memory.
    memset((GCU_Type64_Union *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GCU_Type64_Union));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool gcu_vector64_append(GCU_Vector64 * vector, GCU_Type64_Union value) {
  if ((vector->count >= vector->capacity) && !grow_vector64(vector, vector->count < 32
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

size_t gcu_vector64_count(GCU_Vector64 * vector) {
  return vector->count;
}

GCU_Vector32 * gcu_vector32_create(size_t count) {
  // Malloc Zeroed-out memory.
  GCU_Vector32 * vector = calloc(1, sizeof(GCU_Vector32));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GCU_Type32_Union));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void gcu_vector32_destroy(GCU_Vector32 * vector) {
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

static bool grow_vector32(GCU_Vector32 * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GCU_Vector32 * newTable = gcu_vector32_create(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GCU_Type32_Union)));
  if (newMem) {
    // Zero out the new memory.
    memset((GCU_Type32_Union *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GCU_Type32_Union));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool gcu_vector32_append(GCU_Vector32 * vector, GCU_Type32_Union value) {
  if ((vector->count >= vector->capacity) && !grow_vector32(vector, vector->count < 32
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

size_t gcu_vector32_count(GCU_Vector32 * vector) {
  return vector->count;
}

GCU_Vector16 * gcu_vector16_create(size_t count) {
  // Malloc Zeroed-out memory.
  GCU_Vector16 * vector = calloc(1, sizeof(GCU_Vector16));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GCU_Type16_Union));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void gcu_vector16_destroy(GCU_Vector16 * vector) {
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

static bool grow_vector16(GCU_Vector16 * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GCU_Vector16 * newTable = gcu_vector16_create(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GCU_Type16_Union)));
  if (newMem) {
    // Zero out the new memory.
    memset((GCU_Type16_Union *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GCU_Type16_Union));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool gcu_vector16_append(GCU_Vector16 * vector, GCU_Type16_Union value) {
  if ((vector->count >= vector->capacity) && !grow_vector16(vector, vector->count < 32
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

size_t gcu_vector16_count(GCU_Vector16 * vector) {
  return vector->count;
}

GCU_Vector8 * gcu_vector8_create(size_t count) {
  // Malloc Zeroed-out memory.
  GCU_Vector8 * vector = calloc(1, sizeof(GCU_Vector8));

  // Reserve room for the data, if requested..
  if (count) {
    vector->data = calloc(count, sizeof(GCU_Type8_Union));
    if (vector->data) {
      vector->capacity = count;
    }
  }

  return vector;
}

void gcu_vector8_destroy(GCU_Vector8 * vector) {
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

static bool grow_vector8(GCU_Vector8 * vector, size_t size) {
  // Verify that the pointer actually points to something.
  if (!vector) {
    return false;
  }

  if (size <= vector->capacity) {
    return false;
  }

  GCU_Vector8 * newTable = gcu_vector8_create(size);
  if (!newTable) {
    return false;
  }

  // Attempt to allocate more memory;
  void * newMem = realloc(vector->data, size * (sizeof (GCU_Type8_Union)));
  if (newMem) {
    // Zero out the new memory.
    memset((GCU_Type8_Union *)newMem + vector->capacity, 0, (size - vector->capacity) * sizeof(GCU_Type8_Union));

    // Swap out the details.
    vector->data = newMem;
    vector->capacity = size;
    return true;
  }
  return false;
}

bool gcu_vector8_append(GCU_Vector8 * vector, GCU_Type8_Union value) {
  if ((vector->count >= vector->capacity) && !grow_vector8(vector, vector->count < 32
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

size_t gcu_vector8_count(GCU_Vector8 * vector) {
  return vector->count;
}

