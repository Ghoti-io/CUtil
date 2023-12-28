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

/**
 * Container used to return the result of looking for a hash in the hash table.
 *
 * Although it may seem strange to return a value as part of a structure,
 * especially when the programmer undoubtedly just wants the value, it is also
 * imperitive that the hash table be able to indicate whether or not the value
 * existed in the table.  Both goals are accomplished by this approach.
 */
typedef struct {
  bool exists;          ///< Whether or not the value exists in the hash table.
  GCU_Type_Union value; ///< The value found in the table (if it exists).
} GCU_Hash_Value;

/**
 * Container holding the information for an entry in the hash table.
 *
 * An "entry" is empty (e.g., `occupied = false`) upon creation.  By adding
 * and removing entries from the hash table, the `occupied` and `removed` flags
 * will be changed to track the state of each individual cell.
 */
typedef struct {
  size_t hash;         ///< The hash of the entry.
  GCU_Type_Union data; ///< The data of the entry.
  bool occupied;       ///< Whether or not the entry has been initialized in
                       ///<   some way.
  bool removed;        ///< Whether or not the entry has been removed.
} GCU_Hash_Cell;

/**
 * Container holding the information of the hash table.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the hash table using gcu_hash_create().
 *   2. Destroy the has table using gcu_hash_destory().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the hash table.  The hash
 *      table will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;      ///< The total item capacity of the hash table.
  size_t entries;       ///< The count of non-empty cells.
  size_t removed;       ///< The count of non-empty cells that represent
                        ///<   elements which have been removed.
  GCU_Hash_Cell * data; ///< A pointer to the array of data cells.
} GCU_Hash_Table;

/**
 * A container used to hold the state of an iterator which can be used to
 * traverse all elements of a hash table.
 *
 * A hash table may change internal structure upon adding or removing elements,
 * so any such operations may invalidate the behavior of an iterator.
 *
 * The programmer is responsible to make sure that an iterator is not used
 * improperly after the hash has been modified.
 *
 * An iterator may contain invalid data, in the case where there is no data
 * through which to iterate.  This is indicated by the `exists` field.  The
 * programmer is responsible for checking this field before attempting to
 * use the `value` in any way.
 */
typedef struct {
  size_t current;             ///< The current index into the hashTable data
                              ///<   structure corresponding to the iterator.
  bool exists;                ///< Whether or not the iterator points to valid
                              ///<   data.
  GCU_Type_Union value;       ///< The data pointed to by the iterator.
  GCU_Hash_Table * hashTable; ///< The hash table that the iterator traverses.
} GCU_Hash_Iterator;

/**
 * Create a hash table structure.
 *
 * All invocations of a hash table must have a corresponding gcu_hash_destroy()
 * call in order to clean up dynamically-allocated memory.
 *
 * The hash table will manage the final size of container's memory based on the
 * number of elements that have been added.  The container's memory will be
 * expanded automatically when needed to accomodate new insertions, which can
 * cause an unexpected delay.  Such rebuilding costs can be avoided by proper
 * setting of the `count` variable during creation of the hash table.
 *
 * @param count The number of items anticipated to be stored in the hash table.
 * @return A struct containing the hash table information.
 */
GCU_Hash_Table * gcu_hash_create(size_t count);

/**
 * Destroy a hash table structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param hashTable The hash table structure to be destroyed.
 */
void gcu_hash_destroy(GCU_Hash_Table * hashTable);

/**
 * Set a value in the hash table.
 *
 * Setting a value may trigger a resize of the hash table.  This can be avoided
 * entirely by setting an appropriate `count` value when creating the hash
 * table with gcu_hash_create().
 *
 * @param hashTable The hash table structure on which to operate.
 * @param hash The hash associated with the value.
 * @param value The value to insert into the hash table.
 * @return `true` on success, `false` on failure.
 */
bool gcu_hash_set(GCU_Hash_Table * hashTable, size_t hash, GCU_Type_Union value);

/**
 * Get a value from the hash table (if it exists).
 *
 * @param hashTable The hash table structure on which to operate.
 * @param hash The hash whose associated value will be searched for.
 * @returns A result that indicates the success or failure of the operation, as
 *   well as the associated value (if it exists).
 */
GCU_Hash_Value gcu_hash_get(GCU_Hash_Table * hashTable, size_t hash);

/**
 * Check to see whether or not a hash table contains a specific hash.
 *
 * @param hashTable The hash table structure on which to operate.
 * @param hash The hash whose associated value will be searched for.
 * @return `true` if the hash is in the table, `false` otherwise.
 */
bool gcu_hash_contains(GCU_Hash_Table * hashTable, size_t hash);

/**
 * Remove a hash from the table.
 *
 * The hash table does not manage the values in the table.  Therefore, if an
 * entry is removed from the hash table, then it is up to the programmer to
 * perform any additional work (such as memory cleanup of the value).
 *
 * @param hashTable The hash table structure on which to operate.
 * @param hash The hash whose associated value will be removed from the table.
 * @return `true` if the entry existed and was removed, `false` otherwise.
 */
bool gcu_hash_remove(GCU_Hash_Table * hashTable, size_t hash);

/**
 * Get a count of active entries in the hash table.
 *
 * @param hashTable The hash table structure on which to operate.
 * @return The count of active entries in the hash table.
 */
size_t gcu_hash_count(GCU_Hash_Table * hashTable);

/**
 * Get an iterator which can be used to iterate through the entries of the
 * hash table.
 *
 * @param hashTable The hash table structure on which to operate.
 * @return An iterator pointing to the first element in the hash table (if it
 *   exists).
 */
GCU_Hash_Iterator gcu_hash_iterator_get(GCU_Hash_Table * hashTable);

/**
 * Get an iterator to the next element in the table (if it exists).
 *
 * Any change to the hash table (such as setting a value) might alter the
 * underlying structure of the hash table, which would invalidate the iterator.
 * Any call to gcu_hash_set(), therefore, should be considered as an
 * invalidation of any iterators associated with the hash table.
 *
 * @param iterator The iterator from which to calculate and return the
 *   next iterator.
 * @return An iterator pointing to the next element in the table (if it
 *   exists).
 */
GCU_Hash_Iterator gcu_hash_iterator_next(GCU_Hash_Iterator iterator);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_HASH_H

