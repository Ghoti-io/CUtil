GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _create)(size_t count) {
  // Malloc Zeroed-out memory.
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable = calloc(1, sizeof(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table)));

  // Reserve room for the data, if requested..
  if (count) {
    // We always want the capacity to be an odd number.
    size_t capacity = (count * 2) + 1;
    hashTable->data = calloc(capacity, sizeof(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell)));
    if (hashTable->data) {
      hashTable->capacity = capacity;
    }
  }

  return hashTable;
}

void GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _destroy)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    // Clean up the data table if needed.
    if (hashTable->data) {
      free(hashTable->data);
      hashTable->data = 0;
    }
    free(hashTable);
  }
}

static bool GHOTIIO_CUTIL_CONCAT2(grow_hash, BITDEPTH)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable, size_t size) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  if (size < hashTable->capacity) {
    return false;
  }

  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * newTable = GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _create)(size);
  if (!newTable) {
    return false;
  }

  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = hashTable->data;
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * end = &hashTable->data[hashTable->capacity];

  // Copy data into the new hash table.
  while (cursor != end) {
    if (cursor->occupied && !cursor->removed) {
      GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _set)(newTable, cursor->hash, cursor->data);
    }
    ++cursor;
  }

  // Swap the data.
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) temp = *newTable;
  *newTable = *hashTable;
  *hashTable = temp;

  GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _destroy)(newTable);

  return true;
}

bool GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _set)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable, size_t hash, GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union) value) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  // Grow the hash table if needed.
  if (hashTable->capacity < ((hashTable->entries + 1) * 2)) {
    if (!GHOTIIO_CUTIL_CONCAT2(grow_hash, BITDEPTH)(hashTable, hashTable->capacity < 64
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
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = &hashTable->data[potential_location];

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
  *cursor = (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell)) {
    .hash = hash,
    .data = value,
    .occupied = true,
    .removed = false,
  };
  return true;
}

GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Value) GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _get)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = hashTable->data;
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * end = &hashTable->data[hashTable->capacity];

    // Find the entry, if it exists.
    while (cursor != end) {
      if (cursor->occupied && !cursor->removed && (cursor->hash == hash)) {
        return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Value)) {
          .exists = true,
          .value = cursor->data,
        };
      }
      ++cursor;
    }
  }
  return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Value)) {
    .exists = false,
    .value = (GHOTIIO_CUTIL_CONCAT3(GCU_Type, BITDEPTH, _Union)){0}
  };
}

bool GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _contains)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = hashTable->data;
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * end = &hashTable->data[hashTable->capacity];

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

bool GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _remove)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = &hashTable->data[hash % hashTable->capacity];
    GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * end = &hashTable->data[hashTable->capacity];

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

size_t GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _count)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    // Compute the size.
    return hashTable->entries - hashTable->removed;
  }
  return 0;
}

GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator) GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _iterator_get)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Table) * hashTable) {
  // Verify that the pointer actually points to something and that there is an
  // entry in the hash table.
  if (!hashTable || (hashTable->entries <= hashTable->removed)) {
    return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator)) {
      .current = 0,
      .exists = false,
      .value = DEFAULT_TYPE(0),
      .hashTable = hashTable,
    };
  }

  size_t index = 0;
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = hashTable->data;

  // Find the first entry.
  while (!cursor->occupied || cursor->removed) {
    ++index;
    ++cursor;
  }

  return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator)) {
    .current = index,
    .exists = true,
    .value = cursor->data,
    .hashTable = hashTable,
  };
}

GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator) GHOTIIO_CUTIL_CONCAT3(gcu_hash, BITDEPTH, _iterator_next)(GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator) iterator) {
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * end = &iterator.hashTable->data[iterator.hashTable->capacity];
  size_t index = iterator.current;
  GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Cell) * cursor = &iterator.hashTable->data[index];

  // Find the next entry.
  do {
    ++index;
    ++cursor;
  } while ((cursor != end) && (!cursor->occupied || cursor->removed));

  if (cursor == end) {
    return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator)) {
      .current = index,
      .exists = false,
      .value = DEFAULT_TYPE(0),
      .hashTable = iterator.hashTable,
    };
  }

  return (GHOTIIO_CUTIL_CONCAT3(GCU_Hash, BITDEPTH, _Iterator)) {
    .current = index,
    .exists = true,
    .value = cursor->data,
    .hashTable = iterator.hashTable,
  };
}

