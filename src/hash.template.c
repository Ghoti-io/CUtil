#define TEMPLATE_GROW_HASH         GHOTIIO_CUTIL_CONCAT2(grow_hash, BITDEPTH)
#define TEMPLATE_GCU_HASH          GHOTIIO_CUTIL_CONCAT2(GCU_Hash, BITDEPTH)
#define TEMPLATE_GCU_HASH_ITERATOR GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator)
#define TEMPLATE_GCU_HASH_CELL     GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell)
#define TEMPLATE_GCU_HASH_VALUE    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Value)
#define TEMPLATE_GCU_TYPE_UNION    GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)
#define TEMPLATE_GCU_HASH_CREATE   GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _create)
#define TEMPLATE_GCU_HASH_CREATE_IN_PLACE GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _create_in_place)
#define TEMPLATE_GCU_HASH_DESTROY  GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _destroy)
#define TEMPLATE_GCU_HASH_DESTROY_IN_PLACE GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _destroy_in_place)
#define TEMPLATE_GCU_HASH_CLONE    GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _clone)
#define TEMPLATE_GCU_HASH_SET      GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _set)
#define TEMPLATE_GCU_HASH_GET      GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _get)
#define TEMPLATE_GCU_HASH_CONTAINS GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _contains)
#define TEMPLATE_GCU_HASH_REMOVE   GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _remove)
#define TEMPLATE_GCU_HASH_COUNT    GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _count)
#define TEMPLATE_GCU_HASH_ITERATOR_GET  GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _iterator_get)
#define TEMPLATE_GCU_HASH_ITERATOR_NEXT GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _iterator_next)

TEMPLATE_GCU_HASH * TEMPLATE_GCU_HASH_CREATE(size_t count) {
  // Malloc Zeroed-out memory.
  TEMPLATE_GCU_HASH * hashTable = gcu_calloc(1, sizeof(TEMPLATE_GCU_HASH));

  // If the allocation failed, return null.
  if (!hashTable) {
    return 0;
  }

  if (!TEMPLATE_GCU_HASH_CREATE_IN_PLACE(hashTable, count)) {
    gcu_free(hashTable);
    return 0;
  }

  return hashTable;
}

bool TEMPLATE_GCU_HASH_CREATE_IN_PLACE(TEMPLATE_GCU_HASH * hashTable, size_t count) {
  *hashTable = (TEMPLATE_GCU_HASH) {
    .entries = 0,
    .removed = 0,
    .capacity = 0,
    .data = 0,
    .cleanup = 0,
  };

  // Reserve room for the data, if requested..
  if (count) {
    // We always want the capacity to be an odd number.
    size_t capacity = (count * 2) + 1;
    hashTable->data = gcu_calloc(capacity, sizeof(TEMPLATE_GCU_HASH_CELL));
    if (hashTable->data) {
      hashTable->capacity = capacity;
    }
  }

  // Allocate the mutex.
  bool failure = GCU_MUTEX_CREATE(hashTable->mutex);

  // If the allocation failed, clean up and return null.
  if (failure) {
    if (hashTable->data) {
      gcu_free(hashTable->data);
    }
    return false;
  }

  return true;
}

void TEMPLATE_GCU_HASH_DESTROY(TEMPLATE_GCU_HASH * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    TEMPLATE_GCU_HASH_DESTROY_IN_PLACE(hashTable);
    gcu_free(hashTable);
  }
}

void TEMPLATE_GCU_HASH_DESTROY_IN_PLACE(TEMPLATE_GCU_HASH * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    // Call the `cleanup` function, if it exists.
    if (hashTable->cleanup) {
      hashTable->cleanup(hashTable);
    }

    // Clean up the data table if needed.
    if (hashTable->data) {
      gcu_free(hashTable->data);
      hashTable->data = 0;
    }

    GCU_MUTEX_DESTROY(hashTable->mutex);
  }
}

TEMPLATE_GCU_HASH * TEMPLATE_GCU_HASH_CLONE(TEMPLATE_GCU_HASH * source) {
  // Verify that the pointer actually points to something.
  if (!source) {
    return 0;
  }

  // Create a new hash table and copy all of the source information.
  TEMPLATE_GCU_HASH * newTable = gcu_malloc(sizeof(TEMPLATE_GCU_HASH));
  if (!newTable) {
    return 0;
  }
  memcpy(newTable, source, sizeof(TEMPLATE_GCU_HASH));

  // Copy the data from the source.
  newTable->data = gcu_malloc(source->capacity * sizeof(TEMPLATE_GCU_HASH_CELL));
  if (!newTable->data) {
    gcu_free(newTable);
    return 0;
  }
  memcpy(newTable->data, source->data, source->capacity * sizeof(TEMPLATE_GCU_HASH_CELL));

  // Allocate the mutex.
  bool failure = GCU_MUTEX_CREATE(newTable->mutex);

  // If the allocation failed, clean up and return null.
  if (failure) {
    gcu_free(newTable->data);
    gcu_free(newTable);
    return 0;
  }

  return newTable;
}

static bool TEMPLATE_GROW_HASH(TEMPLATE_GCU_HASH * hashTable, size_t size) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  if (size < hashTable->capacity) {
    return false;
  }

  TEMPLATE_GCU_HASH * newTable = TEMPLATE_GCU_HASH_CREATE(size);
  if (!newTable) {
    return false;
  }

  TEMPLATE_GCU_HASH_CELL * cursor = hashTable->data;
  TEMPLATE_GCU_HASH_CELL * end = &hashTable->data[hashTable->capacity];

  // Copy data into the new hash table.
  while (cursor != end) {
    if (cursor->occupied && !cursor->removed) {
      TEMPLATE_GCU_HASH_SET(newTable, cursor->hash, cursor->data);
    }
    ++cursor;
  }

  // Swap the data.
  TEMPLATE_GCU_HASH temp = *newTable;
  *newTable = *hashTable;
  *hashTable = temp;

  TEMPLATE_GCU_HASH_DESTROY(newTable);

  return true;
}

bool TEMPLATE_GCU_HASH_SET(TEMPLATE_GCU_HASH * hashTable, size_t hash, TEMPLATE_GCU_TYPE_UNION value) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  // Grow the hash table if needed.
  if (hashTable->capacity < ((hashTable->entries + 1) * 2)) {
    if (!TEMPLATE_GROW_HASH(hashTable, hashTable->capacity < 64
          ? 32
          : hashTable->capacity < 1024
            ? (hashTable->capacity * 2)
            : (hashTable->capacity * GROWTH_FACTOR))) {
      // The hash table could not grow for some reason.
      return false;
    }
  }

  size_t capacity = hashTable->capacity;
  size_t potential_location = hash % capacity;

  // Look for a viable location.

  // First, determine wether or not there is an active entry for this hash in
  // the table that we can reuse.
  // Along the way, make a note of the first "removed" entry, which we may fall
  // back to if there is no active entry.
  size_t fallback_location = capacity;
  TEMPLATE_GCU_HASH_CELL * cursor = &hashTable->data[potential_location];

  // We are not protecting against an infinite loop, because at this point we
  // know that the capacity is larger than the size, and therefore an infinite
  // loop is impossible.
  while (cursor->occupied && (cursor->hash != hash)) {
    if ((fallback_location == capacity) && cursor->occupied && cursor->removed) {
      fallback_location = potential_location;
    }
    ++potential_location;
    ++cursor;
    if (potential_location == capacity) {
      potential_location = 0;
      cursor = hashTable->data;
    }
  }

  // If `cursor` not is active and of the correct hash, then replace the data.
  // If this is not setting an already existing, active entry, then figure out
  // where we want to write the data, either cursor or the fallback_location.
  if (!(cursor->occupied && (cursor->hash == hash) && !cursor->removed)) {
    // If there is a fallback_location, then use it.  Otherwise, use the cursor.
    if (fallback_location < capacity) {
      cursor = &hashTable->data[fallback_location];
    }

    // Adjust the recordkeeping counts as necessary.
    if (cursor->occupied) {
      // This must be a "removed" cell that is being reused.
      --hashTable->removed;
    }
    else {
      ++hashTable->entries;
    }
  }

  // Finally, write the data.
  *cursor = (TEMPLATE_GCU_HASH_CELL) {
    .hash = hash,
    .data = value,
    .occupied = true,
    .removed = false,
  };
  return true;
}

TEMPLATE_GCU_HASH_VALUE TEMPLATE_GCU_HASH_GET(TEMPLATE_GCU_HASH * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    TEMPLATE_GCU_HASH_CELL * cursor = hashTable->data;
    TEMPLATE_GCU_HASH_CELL * end = &hashTable->data[hashTable->capacity];

    // Find the entry, if it exists.
    while (cursor != end) {
      if (cursor->occupied && !cursor->removed && (cursor->hash == hash)) {
        return (TEMPLATE_GCU_HASH_VALUE) {
          .exists = true,
          .value = cursor->data,
        };
      }
      ++cursor;
    }
  }
  return (TEMPLATE_GCU_HASH_VALUE) {
    .exists = false,
    .value = (TEMPLATE_GCU_TYPE_UNION){0}
  };
}

bool TEMPLATE_GCU_HASH_CONTAINS(TEMPLATE_GCU_HASH * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    TEMPLATE_GCU_HASH_CELL * cursor = hashTable->data;
    TEMPLATE_GCU_HASH_CELL * end = &hashTable->data[hashTable->capacity];

    // Find the entry, if it exists.
    while (cursor != end) {
      if (cursor->occupied && !cursor->removed && (cursor->hash == hash)) {
        return true;
      }
      ++cursor;
    }
  }
  return false;
}

bool TEMPLATE_GCU_HASH_REMOVE(TEMPLATE_GCU_HASH * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    TEMPLATE_GCU_HASH_CELL * cursor = &hashTable->data[hash % hashTable->capacity];
    TEMPLATE_GCU_HASH_CELL * end = &hashTable->data[hashTable->capacity];

    // Find the entry to remove.
    while (cursor->occupied) {
      if (!cursor->removed && (cursor->hash == hash)) {
        cursor->removed = true;
        ++hashTable->removed;
        return true;
      }
      ++cursor;
      if (cursor == end) {
        cursor = hashTable->data;
      }
    }
  }
  return false;
}

size_t TEMPLATE_GCU_HASH_COUNT(TEMPLATE_GCU_HASH * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    // Compute the size.
    return hashTable->entries - hashTable->removed;
  }
  return 0;
}

TEMPLATE_GCU_HASH_ITERATOR TEMPLATE_GCU_HASH_ITERATOR_GET(TEMPLATE_GCU_HASH * hashTable) {
  // Verify that the pointer actually points to something and that there is an
  // entry in the hash table.
  if (!hashTable || (hashTable->entries <= hashTable->removed)) {
    return (TEMPLATE_GCU_HASH_ITERATOR) {
      .current = 0,
      .exists = false,
      .hash = 0,
      .value = DEFAULT_TYPE(0),
      .hashTable = hashTable,
    };
  }

  size_t index = 0;
  TEMPLATE_GCU_HASH_CELL * cursor = hashTable->data;

  // Find the first entry.
  while (!cursor->occupied || cursor->removed) {
    ++index;
    ++cursor;
  }

  return (TEMPLATE_GCU_HASH_ITERATOR) {
    .current = index,
    .exists = true,
    .hash = cursor->hash,
    .value = cursor->data,
    .hashTable = hashTable,
  };
}

TEMPLATE_GCU_HASH_ITERATOR TEMPLATE_GCU_HASH_ITERATOR_NEXT(TEMPLATE_GCU_HASH_ITERATOR iterator) {
  TEMPLATE_GCU_HASH_CELL * end = &iterator.hashTable->data[iterator.hashTable->capacity];
  size_t index = iterator.current;
  TEMPLATE_GCU_HASH_CELL * cursor = &iterator.hashTable->data[index];

  // Find the next entry.
  do {
    ++index;
    ++cursor;
  } while ((cursor != end) && (!cursor->occupied || cursor->removed));

  if (cursor == end) {
    return (TEMPLATE_GCU_HASH_ITERATOR) {
      .current = index,
      .exists = false,
      .hash = 0,
      .value = DEFAULT_TYPE(0),
      .hashTable = iterator.hashTable,
    };
  }

  return (TEMPLATE_GCU_HASH_ITERATOR) {
    .current = index,
    .exists = true,
    .hash = cursor->hash,
    .value = cursor->data,
    .hashTable = iterator.hashTable,
  };
}

#undef TEMPLATE_GROW_HASH
#undef TEMPLATE_GCU_HASH
#undef TEMPLATE_GCU_HASH_ITERATOR
#undef TEMPLATE_GCU_HASH_CELL
#undef TEMPLATE_GCU_HASH_VALUE
#undef TEMPLATE_GCU_TYPE_UNION
#undef TEMPLATE_GCU_HASH_CREATE
#undef TEMPLATE_GCU_HASH_CREATE_IN_PLACE
#undef TEMPLATE_GCU_HASH_DESTROY
#undef TEMPLATE_GCU_HASH_DESTROY_IN_PLACE
#undef TEMPLATE_GCU_HASH_CLONE
#undef TEMPLATE_GCU_HASH_SET
#undef TEMPLATE_GCU_HASH_GET
#undef TEMPLATE_GCU_HASH_CONTAINS
#undef TEMPLATE_GCU_HASH_REMOVE
#undef TEMPLATE_GCU_HASH_COUNT
#undef TEMPLATE_GCU_HASH_ITERATOR_GET
#undef TEMPLATE_GCU_HASH_ITERATOR_NEXT

