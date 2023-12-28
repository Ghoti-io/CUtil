/**
 * @file
 * A simple hash table implementation.
 */

#ifndef GHOTIIO_CUTIL_HASH_H
#define GHOTIIO_CUTIL_HASH_H

#include <stddef.h>
#include "cutil/type.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define GCU_Hash_Value GHOTIIO_CUTIL(GCU_Hash_Value)
#define GCU_Hash_Cell GHOTIIO_CUTIL(GCU_Hash_Cell)
#define GCU_Hash_Table GHOTIIO_CUTIL(GCU_Hash_Table)
#define GCU_Hash_Iterator GHOTIIO_CUTIL(GCU_Hash_Iterator)
#define gcu_hash_create GHOTIIO_CUTIL(gcu_hash_create)
#define gcu_hash_destroy GHOTIIO_CUTIL(gcu_hash_destroy)
#define gcu_hash_set GHOTIIO_CUTIL(gcu_hash_set)
#define gcu_hash_get GHOTIIO_CUTIL(gcu_hash_get)
#define gcu_hash_contains GHOTIIO_CUTIL(gcu_hash_contains)
#define gcu_hash_remove GHOTIIO_CUTIL(gcu_hash_remove)
#define gcu_hash_count GHOTIIO_CUTIL(gcu_hash_count)
#define gcu_hash_iterator_get GHOTIIO_CUTIL(gcu_hash_iterator_get)
#define gcu_hash_iterator_next GHOTIIO_CUTIL(gcu_hash_iterator_next)
/// @endcond

typedef struct {
  bool exists;
  GCU_Type_Union value;
} GCU_Hash_Value;

typedef struct {
  size_t hash;
  GCU_Type_Union data;
  bool occupied;
  bool removed;
} GCU_Hash_Cell;

typedef struct {
  size_t capacity;
  size_t entries;
  size_t removed;
  GCU_Hash_Cell * data;
} GCU_Hash_Table;

typedef struct {
  size_t current;
  bool exists;
  GCU_Type_Union value;
  GCU_Hash_Table * hashTable;
} GCU_Hash_Iterator;

GCU_Hash_Table * gcu_hash_create(size_t capacity);

void gcu_hash_destroy(GCU_Hash_Table * hashTable);

bool gcu_hash_set(GCU_Hash_Table * hashTable, size_t hash, GCU_Type_Union value);

GCU_Hash_Value gcu_hash_get(GCU_Hash_Table * hashTable, size_t hash);

bool gcu_hash_contains(GCU_Hash_Table * hashTable, size_t hash);

bool gcu_hash_remove(GCU_Hash_Table * hashTable, size_t hash);

size_t gcu_hash_count(GCU_Hash_Table * hashTable);

GCU_Hash_Iterator gcu_hash_iterator_get(GCU_Hash_Table * hashTable);

GCU_Hash_Iterator gcu_hash_iterator_next(GCU_Hash_Iterator iterator);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_HASH_H

