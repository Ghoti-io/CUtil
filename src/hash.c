/**
 * @file
 */

#include <stdlib.h>
#include "cutil/hash.h"

#define GROWTH_FACTOR 1.25

GCU_Hash_Table * gcu_hash_create(size_t capacity) {
  // Malloc Zeroed-out memory.
  GCU_Hash_Table * hashTable = calloc(1, sizeof(GCU_Hash_Table));

  // Reserve room for the data, if requested..
  if (capacity) {
    // We always want the capacity to be an odd number.
    size_t newCapacity = (capacity * 2) + 1;
    hashTable->data = calloc(newCapacity, sizeof(GCU_Hash_Cell));
    if (hashTable->data) {
      hashTable->capacity = newCapacity;
    }
  }

  return hashTable;
}

void gcu_hash_destroy(GCU_Hash_Table * hashTable) {
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

static bool grow_hash(GCU_Hash_Table * hashTable, size_t size) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  if (size < hashTable->capacity) {
    return false;
  }

  GCU_Hash_Table * newTable = gcu_hash_create(size);
  if (!newTable) {
    return false;
  }

  GCU_Hash_Cell * cursor = hashTable->data;
  GCU_Hash_Cell * end = &hashTable->data[hashTable->capacity];

  // Copy data into the new hash table.
  while (cursor != end) {
    if (cursor->occupied && !cursor->removed) {
      gcu_hash_set(newTable, cursor->hash, cursor->data);
    }
    ++cursor;
  }

  // Swap the data.
  GCU_Hash_Table temp = *newTable;
  *newTable = *hashTable;
  *hashTable = temp;

  gcu_hash_destroy(newTable);

  return true;
}

bool gcu_hash_set(GCU_Hash_Table * hashTable, size_t hash, GCU_Type_Union value) {
  // Verify that the pointer actually points to something.
  if (!hashTable) {
    return false;
  }

  // Grow the hash table if needed.
  if (hashTable->capacity < ((hashTable->entries + 1) * 2)) {
    if (!grow_hash(hashTable, hashTable->capacity < 64
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
  GCU_Hash_Cell * cursor = &hashTable->data[potential_location];

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
  *cursor = (GCU_Hash_Cell) {
    .hash = hash,
    .data = value,
    .occupied = true,
    .removed = false,
  };
  return true;
}

GCU_Hash_Value gcu_hash_get(GCU_Hash_Table * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GCU_Hash_Cell * cursor = hashTable->data;
    GCU_Hash_Cell * end = &hashTable->data[hashTable->capacity];

    // Find the entry, if it exists.
    while (cursor != end) {
      if (cursor->occupied && !cursor->removed && (cursor->hash == hash)) {
        return (GCU_Hash_Value) {
          .exists = true,
          .value = cursor->data,
        };
      }
      ++cursor;
    }
  }
  return (GCU_Hash_Value) {
    .exists = false,
    .value = (GCU_Type_Union){0}
  };
}

bool gcu_hash_contains(GCU_Hash_Table * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GCU_Hash_Cell * cursor = hashTable->data;
    GCU_Hash_Cell * end = &hashTable->data[hashTable->capacity];

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

bool gcu_hash_remove(GCU_Hash_Table * hashTable, size_t hash) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    GCU_Hash_Cell * cursor = &hashTable->data[hash % hashTable->capacity];
    GCU_Hash_Cell * end = &hashTable->data[hashTable->capacity];

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

size_t gcu_hash_count(GCU_Hash_Table * hashTable) {
  // Verify that the pointer actually points to something.
  if (hashTable) {
    // Compute the size.
    return hashTable->entries - hashTable->removed;
  }
  return 0;
}

GCU_Hash_Iterator gcu_hash_iterator_get(GCU_Hash_Table * hashTable) {
  // Verify that the pointer actually points to something and that there is an
  // entry in the hash table.
  if (!hashTable || (hashTable->entries <= hashTable->removed)) {
    return (GCU_Hash_Iterator) {
      .current = 0,
      .exists = false,
      .value = gcu_type_ui32(0),
      .hashTable = hashTable,
    };
  }

  size_t index = 0;
  GCU_Hash_Cell * cursor = hashTable->data;

  // Find the first entry.
  while (!cursor->occupied || cursor->removed) {
    ++index;
    ++cursor;
  }

  return (GCU_Hash_Iterator) {
    .current = index,
    .exists = true,
    .value = cursor->data,
    .hashTable = hashTable,
  };
}

GCU_Hash_Iterator gcu_hash_iterator_next(GCU_Hash_Iterator iterator) {
  GCU_Hash_Cell * end = &iterator.hashTable->data[iterator.hashTable->capacity];
  size_t index = iterator.current;
  GCU_Hash_Cell * cursor = &iterator.hashTable->data[index];

  // Find the next entry.
  do {
    ++index;
    ++cursor;
  } while ((cursor != end) && (!cursor->occupied || cursor->removed));

  if (cursor == end) {
    return (GCU_Hash_Iterator) {
      .current = index,
      .exists = false,
      .value = gcu_type_ui32(0),
      .hashTable = iterator.hashTable,
    };
  }

  return (GCU_Hash_Iterator) {
    .current = index,
    .exists = true,
    .value = cursor->data,
    .hashTable = iterator.hashTable,
  };
}

