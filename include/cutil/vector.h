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
#define GCU_Vector64_Value GHOTIIO_CUTIL(GCU_Vector64_Value)
#define GCU_Vector64 GHOTIIO_CUTIL(GCU_Vector64)
#define gcu_vector64_create GHOTIIO_CUTIL(gcu_vector64_create)
#define gcu_vector64_destroy GHOTIIO_CUTIL(gcu_vector64_destroy)
#define gcu_vector64_append GHOTIIO_CUTIL(gcu_vector64_remove)
#define gcu_vector64_count GHOTIIO_CUTIL(gcu_vector64_count)
#define GCU_Vector32_Value GHOTIIO_CUTIL(GCU_Vector32_Value)
#define GCU_Vector32 GHOTIIO_CUTIL(GCU_Vector32)
#define gcu_vector32_create GHOTIIO_CUTIL(gcu_vector32_create)
#define gcu_vector32_destroy GHOTIIO_CUTIL(gcu_vector32_destroy)
#define gcu_vector32_append GHOTIIO_CUTIL(gcu_vector32_remove)
#define gcu_vector32_count GHOTIIO_CUTIL(gcu_vector32_count)
#define GCU_Vector16_Value GHOTIIO_CUTIL(GCU_Vector16_Value)
#define GCU_Vector16 GHOTIIO_CUTIL(GCU_Vector16)
#define gcu_vector16_create GHOTIIO_CUTIL(gcu_vector16_create)
#define gcu_vector16_destroy GHOTIIO_CUTIL(gcu_vector16_destroy)
#define gcu_vector16_append GHOTIIO_CUTIL(gcu_vector16_remove)
#define gcu_vector16_count GHOTIIO_CUTIL(gcu_vector16_count)
#define GCU_Vector8_Value GHOTIIO_CUTIL(GCU_Vector8_Value)
#define GCU_Vector8 GHOTIIO_CUTIL(GCU_Vector8)
#define gcu_vector8_create GHOTIIO_CUTIL(gcu_vector8_create)
#define gcu_vector8_destroy GHOTIIO_CUTIL(gcu_vector8_destroy)
#define gcu_vector8_append GHOTIIO_CUTIL(gcu_vector8_remove)
#define gcu_vector8_count GHOTIIO_CUTIL(gcu_vector8_count)
/// @endcond

/**
 * Container holding the information of the 64-bit vector.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the vector using gcu_vector64_create().
 *   2. Destroy the vector using gcu_vector64_destroy().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the vector.  The vector
 *      will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;       ///< The total item capacity of the vector.
  size_t count;          ///< The count of non-empty cells.
  GCU_Type64_Union * data; ///< A pointer to the array of data cells.
} GCU_Vector64;

/**
 * Create a vector structure.
 *
 * All invocations of a vector must have a corresponding gcu_vector64_destroy()
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
GCU_Vector64 * gcu_vector64_create(size_t count);

/**
 * Destroy a vector structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param vector The vector structure to be destroyed.
 */
void gcu_vector64_destroy(GCU_Vector64 * vector);

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
bool gcu_vector64_append(GCU_Vector64 * vector, GCU_Type64_Union value);

/**
 * Get a count of entries in the vector.
 *
 * @param vector The vector structure on which to operate.
 * @return The count of entries in the vector.
 */
size_t gcu_vector64_count(GCU_Vector64 * vector);

/**
 * Container holding the information of the 32-bit vector.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the vector using gcu_vector32_create().
 *   2. Destroy the vector using gcu_vector32_destroy().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the vector.  The vector
 *      will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;       ///< The total item capacity of the vector.
  size_t count;          ///< The count of non-empty cells.
  GCU_Type32_Union * data; ///< A pointer to the array of data cells.
} GCU_Vector32;

/**
 * Create a vector structure.
 *
 * All invocations of a vector must have a corresponding gcu_vector32_destroy()
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
GCU_Vector32 * gcu_vector32_create(size_t count);

/**
 * Destroy a vector structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param vector The vector structure to be destroyed.
 */
void gcu_vector32_destroy(GCU_Vector32 * vector);

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
bool gcu_vector32_append(GCU_Vector32 * vector, GCU_Type32_Union value);

/**
 * Get a count of entries in the vector.
 *
 * @param vector The vector structure on which to operate.
 * @return The count of entries in the vector.
 */
size_t gcu_vector32_count(GCU_Vector32 * vector);

/**
 * Container holding the information of the 16-bit vector.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the vector using gcu_vector16_create().
 *   2. Destroy the vector using gcu_vector16_destroy().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the vector.  The vector
 *      will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;       ///< The total item capacity of the vector.
  size_t count;          ///< The count of non-empty cells.
  GCU_Type16_Union * data; ///< A pointer to the array of data cells.
} GCU_Vector16;

/**
 * Create a vector structure.
 *
 * All invocations of a vector must have a corresponding gcu_vector16_destroy()
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
GCU_Vector16 * gcu_vector16_create(size_t count);

/**
 * Destroy a vector structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param vector The vector structure to be destroyed.
 */
void gcu_vector16_destroy(GCU_Vector16 * vector);

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
bool gcu_vector16_append(GCU_Vector16 * vector, GCU_Type16_Union value);

/**
 * Get a count of entries in the vector.
 *
 * @param vector The vector structure on which to operate.
 * @return The count of entries in the vector.
 */
size_t gcu_vector16_count(GCU_Vector16 * vector);

/**
 * Container holding the information of the 8-bit vector.
 *
 * For proper memory management, the programmer is responsible for 4 things:
 *   1. Initialize the vector using gcu_vector8_create().
 *   2. Destroy the vector using gcu_vector8_destroy().
 *   3. Implementation of any thread-safety synchronization.
 *   4. Life cycle management of the contents of the vector.  The vector
 *      will **not**, for example, attempt to manage any pointers that it
 *      may contain upon deletion.  The programmer is responsible for all
 *      memory management.
 */
typedef struct {
  size_t capacity;       ///< The total item capacity of the vector.
  size_t count;          ///< The count of non-empty cells.
  GCU_Type8_Union * data; ///< A pointer to the array of data cells.
} GCU_Vector8;

/**
 * Create a vector structure.
 *
 * All invocations of a vector must have a corresponding gcu_vector8_destroy()
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
GCU_Vector8 * gcu_vector8_create(size_t count);

/**
 * Destroy a vector structure and clean up memory allocations.
 *
 * This function will not address any memory allocations of the elements
 * themselves (if any).  The programmer is responsible for controlling any
 * memory management on behalf of the elements.
 *
 * @param vector The vector structure to be destroyed.
 */
void gcu_vector8_destroy(GCU_Vector8 * vector);

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
bool gcu_vector8_append(GCU_Vector8 * vector, GCU_Type8_Union value);

/**
 * Get a count of entries in the vector.
 *
 * @param vector The vector structure on which to operate.
 * @return The count of entries in the vector.
 */
size_t gcu_vector8_count(GCU_Vector8 * vector);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_VECTOR_H

