/**
 * @file
 * Type definitions and utilities for use by the Ghoti.io projects.
 */

#ifndef GHOTIIO_CUTIL_TYPE_H
#define GHOTIIO_CUTIL_TYPE_H

#include <stdbool.h>
#include <stdint.h>
#include "cutil/libver.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define GCU_Type_Union GHOTIIO_CUTIL(GCU_Type_Union)
#define gcu_type_pointer GHOTIIO_CUTIL(gcu_type_pointer)
#define gcu_type_ui64 GHOTIIO_CUTIL(gcu_type_ui64)
#define gcu_type_ui32 GHOTIIO_CUTIL(gcu_type_ui32)
#define gcu_type_ui16 GHOTIIO_CUTIL(gcu_type_ui16)
#define gcu_type_ui8 GHOTIIO_CUTIL(gcu_type_ui8)
#define gcu_type_i64 GHOTIIO_CUTIL(gcu_type_i64)
#define gcu_type_i32 GHOTIIO_CUTIL(gcu_type_i32)
#define gcu_type_i16 GHOTIIO_CUTIL(gcu_type_i16)
#define gcu_type_i8 GHOTIIO_CUTIL(gcu_type_i8)
#define gcu_type_char GHOTIIO_CUTIL(gcu_type_char)
/// @endcond

/**
 * A union of all basic types to be used by generic containers.
 */
typedef union {
  void * p;
  uint64_t ui64;
  uint32_t ui32;
  uint16_t ui16;
  uint8_t ui8;
  int64_t i64;
  int32_t i32;
  int16_t i16;
  int8_t i8;
  char c;
} GCU_Type_Union;

/**
 * Create a union variable with the type `void *`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_p(void * val);

/**
 * Create a union variable with the type `uint64_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_ui64(uint64_t val);

/**
 * Create a union variable with the type `uint32_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_ui32(uint32_t val);

/**
 * Create a union variable with the type `uint16_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_ui16(uint16_t val);

/**
 * Create a union variable with the type `uint8_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_ui8(uint8_t val);

/**
 * Create a union variable with the type `int64_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_i64(int64_t val);

/**
 * Create a union variable with the type `int32_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_i32(int32_t val);

/**
 * Create a union variable with the type `int16_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_i16(int16_t val);

/**
 * Create a union variable with the type `int8_t`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_i8(int8_t val);

/**
 * Create a union variable with the type `char`.
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type_Union gcu_type_c(char val);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_TYPE_H

