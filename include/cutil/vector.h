/**
 * @file
 * A simple vector implementation.
 */

#ifndef GHOTIIO_CUTIL_VECTOR_H
#define GHOTIIO_CUTIL_VECTOR_H

#include <stddef.h>
#include "cutil/type.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define GCU_Vector_Value GHOTIIO_CUTIL(GCU_Vector_Value)
#define GCU_Vector GHOTIIO_CUTIL(GCU_Vector)
#define gcu_vector_create GHOTIIO_CUTIL(gcu_vector_create)
#define gcu_vector_destroy GHOTIIO_CUTIL(gcu_vector_destroy)
#define gcu_vector_append GHOTIIO_CUTIL(gcu_vector_remove)
#define gcu_vector_count GHOTIIO_CUTIL(gcu_vector_count)
/// @endcond

/**
 * Container holding the information of the vector.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the vector using gcu_vector_create().
 *   2. Destroy the vector using gcu_vector_destory().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the vector.  The vector
 *      will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;       ///< The total item capacity of the vector.
  size_t count;          ///< The count of non-empty cells.
  GCU_Type_Union * data; ///< A pointer to the array of data cells.
} GCU_Vector;

/**
 * Create a vector structure.
 *
 * All invocations of a vector must have a corresponding gcu_vector_destroy()
 * call in order to clean up dynamically-allocated memory.
 *
 * The vector will manage the final size of container's memory based on the
 * number of elements that have been added.  The container's memory will be
 * expanded automatically when needed to accomodate new insertions, which can
 * cause an unexpected delay.  Such rebuilding costs can be avoided by proper
 * setting of the `count` variable during creation of the vector.
 *
 * @param count The number of items anticipated to be stored in the vector.
 * @return A struct containing the vector information.
 */
GCU_Vector * gcu_vector_create(size_t count);

/**
 * Destroy a vector structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param vector The vector structure to be destroyed.
 */
void gcu_vector_destroy(GCU_Vector * vector);

/**
 * Append an item at the end of the vector.
 *
 * If there is not enough space in the current data structure, new space will
 * be attempted to be allocated.  This may invalidate any pointers to the
 * previous data locations.
 *
 * @param value The item to append to the end of the vector.
 * @return `true` on success, `false` otherwise.
 */
bool gcu_vector_append(GCU_Vector * vector, GCU_Type_Union value);

/**
 * Get a count of entries in the vector.
 *
 * @param vector The vector structure on which to operate.
 * @return The count of entries in the vector.
 */
size_t gcu_vector_count(GCU_Vector * vector);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_VECTOR_H

