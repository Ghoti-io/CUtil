/**
 * @file
 * Type definitions and utilities for use by the Ghoti.io projects.
 */

#ifndef GHOTIIO_CUTIL_TYPE_H
#define GHOTIIO_CUTIL_TYPE_H

#include <stdbool.h>
#include <stdint.h>
#include <cutil/libver.h>
#include <cutil/float.h>

#ifdef __cplusplus
extern "C" {
#endif

/// @cond HIDDEN_SYMBOLS
#define GCU_Type64_Union GHOTIIO_CUTIL(GCU_Type64_Union)
#define GCU_Type32_Union GHOTIIO_CUTIL(GCU_Type32_Union)
#define GCU_Type16_Union GHOTIIO_CUTIL(GCU_Type16_Union)
#define GCU_Type8_Union GHOTIIO_CUTIL(GCU_Type8_Union)

#define gcu_type64_p GHOTIIO_CUTIL(gcu_type64_p)
#define gcu_type64_ui64 GHOTIIO_CUTIL(gcu_type64_ui64)
#define gcu_type64_ui32 GHOTIIO_CUTIL(gcu_type64_ui32)
#define gcu_type64_ui16 GHOTIIO_CUTIL(gcu_type64_ui16)
#define gcu_type64_ui8 GHOTIIO_CUTIL(gcu_type64_ui8)
#define gcu_type64_i64 GHOTIIO_CUTIL(gcu_type64_i64)
#define gcu_type64_i32 GHOTIIO_CUTIL(gcu_type64_i32)
#define gcu_type64_i16 GHOTIIO_CUTIL(gcu_type64_i16)
#define gcu_type64_i8 GHOTIIO_CUTIL(gcu_type64_i8)
#define gcu_type64_wc GHOTIIO_CUTIL(gcu_type64_wc)
#define gcu_type64_c GHOTIIO_CUTIL(gcu_type64_c)
#define gcu_type64_b GHOTIIO_CUTIL(gcu_type64_b)

#define gcu_type32_ui32 GHOTIIO_CUTIL(gcu_type32_ui32)
#define gcu_type32_ui16 GHOTIIO_CUTIL(gcu_type32_ui16)
#define gcu_type32_ui8 GHOTIIO_CUTIL(gcu_type32_ui8)
#define gcu_type32_i32 GHOTIIO_CUTIL(gcu_type32_i32)
#define gcu_type32_i16 GHOTIIO_CUTIL(gcu_type32_i16)
#define gcu_type32_i8 GHOTIIO_CUTIL(gcu_type32_i8)
#if GCU_WCHAR_WIDTH <= 4
#define gcu_type32_wc GHOTIIO_CUTIL(gcu_type32_wc)
#endif
#define gcu_type32_c GHOTIIO_CUTIL(gcu_type32_c)
#define gcu_type32_b GHOTIIO_CUTIL(gcu_type32_b)

#define gcu_type16_ui16 GHOTIIO_CUTIL(gcu_type16_ui16)
#define gcu_type16_ui8 GHOTIIO_CUTIL(gcu_type16_ui8)
#define gcu_type16_i16 GHOTIIO_CUTIL(gcu_type16_i16)
#define gcu_type16_i8 GHOTIIO_CUTIL(gcu_type16_i8)
#define gcu_type16_c GHOTIIO_CUTIL(gcu_type16_c)
#define gcu_type16_b GHOTIIO_CUTIL(gcu_type16_b)

#define gcu_type8_ui8 GHOTIIO_CUTIL(gcu_type8_ui8)
#define gcu_type8_i8 GHOTIIO_CUTIL(gcu_type8_i8)
#define gcu_type8_c GHOTIIO_CUTIL(gcu_type8_c)
#define gcu_type8_b GHOTIIO_CUTIL(gcu_type8_b)
/// @endcond

/**
 * Create a 64-bit union variable with the type `void *`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_p()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_P(val) ((GCU_Type64_Union) {.p = val})

/**
 * Create a 64-bit union variable with the type `uint64_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_ui64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_UI64(val) ((GCU_Type64_Union) {.ui64 = val})

/**
 * Create a 64-bit union variable with the type `uint32_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_ui32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_UI32(val) ((GCU_Type64_Union) {.ui32 = val})

/**
 * Create a 64-bit union variable with the type `uint16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_ui16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_UI16(val) ((GCU_Type64_Union) {.ui16 = val})

/**
 * Create a 64-bit union variable with the type `uint8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_ui8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_UI8(val) ((GCU_Type64_Union) {.ui8 = val})

/**
 * Create a 64-bit union variable with the type `int64_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_i64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_I64(val) ((GCU_Type64_Union) {.i64 = val})

/**
 * Create a 64-bit union variable with the type `int32_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_i32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_I32(val) ((GCU_Type64_Union) {.i32 = val})

/**
 * Create a 64-bit union variable with the type `int16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_i16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_I16(val) ((GCU_Type64_Union) {.i16 = val})

/**
 * Create a 64-bit union variable with the type `int8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_i8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_I8(val) ((GCU_Type64_Union) {.i8 = val})

/**
 * Create a 64-bit union variable with the type float with 64 bits.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_f64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_F64(val) ((GCU_Type64_Union) {.f64 = val})

/**
 * Create a 64-bit union variable with the type float with 32 bits.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_f32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_F32(val) ((GCU_Type64_Union) {.f32 = val})

/**
 * Create a 64-bit union variable with the type `wchar_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_wc()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_WC(val) ((GCU_Type64_Union) {.wc = val})

/**
 * Create a 64-bit union variable with the type `char`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_c()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_C(val) ((GCU_Type64_Union) {.c = val})

/**
 * Create a 64-bit union variable with the type `bool`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type64_b()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE64_B(val) ((GCU_Type64_Union) {.b = val})

/**
 * Create a 32-bit union variable with the type `uint32_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_ui32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_UI32(val) ((GCU_Type32_Union) {.ui32 = val})

/**
 * Create a 32-bit union variable with the type `uint16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_ui16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_UI16(val) ((GCU_Type32_Union) {.ui16 = val})

/**
 * Create a 32-bit union variable with the type `uint8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_ui8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_UI8(val) ((GCU_Type32_Union) {.ui8 = val})

/**
 * Create a 32-bit union variable with the type `int32_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_i32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_I32(val) ((GCU_Type32_Union) {.i32 = val})

/**
 * Create a 32-bit union variable with the type `int16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_i16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_I16(val) ((GCU_Type32_Union) {.i16 = val})

/**
 * Create a 32-bit union variable with the type `int8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_i8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_I8(val) ((GCU_Type32_Union) {.i8 = val})

/**
 * Create a 32-bit union variable with the type float with 32 bits.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_f32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_F32(val) ((GCU_Type32_Union) {.f32 = val})

#if GCU_WCHAR_WIDTH <= 4
/**
 * Create a 32-bit union variable with the type `wchar_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_wc()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_WC(val) ((GCU_Type32_Union) {.wc = val})
#endif

/**
 * Create a 32-bit union variable with the type `char`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_c()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_C(val) ((GCU_Type32_Union) {.c = val})

/**
 * Create a 32-bit union variable with the type `bool`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type32_b()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE32_B(val) ((GCU_Type32_Union) {.b = val})

/**
 * Create a 16-bit union variable with the type `uint16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_ui16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_UI16(val) ((GCU_Type16_Union) {.ui16 = val})

/**
 * Create a 16-bit union variable with the type `uint8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_ui8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_UI8(val) ((GCU_Type16_Union) {.ui8 = val})

/**
 * Create a 16-bit union variable with the type `int16_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_i16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_I16(val) ((GCU_Type16_Union) {.i16 = val})

/**
 * Create a 16-bit union variable with the type `int8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_i8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_I8(val) ((GCU_Type16_Union) {.i8 = val})

/**
 * Create a 16-bit union variable with the type `char`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_c()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_C(val) ((GCU_Type16_Union) {.c = val})

/**
 * Create a 16-bit union variable with the type `bool`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type16_b()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE16_B(val) ((GCU_Type16_Union) {.b = val})

/**
 * Create a 8-bit union variable with the type `uint8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type8_ui8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE8_UI8(val) ((GCU_Type8_Union) {.ui8 = val})

/**
 * Create a 8-bit union variable with the type `int8_t`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type8_i8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE8_I8(val) ((GCU_Type8_Union) {.i8 = val})

/**
 * Create a 8-bit union variable with the type `char`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type8_c()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE8_C(val) ((GCU_Type8_Union) {.c = val})

/**
 * Create a 8-bit union variable with the type `bool`.
 *
 * This `#define` is a compound literal.  It is allowed in C but not C++.
 * There is a corresponding function for use in C++.
 *
 * @see gcu_type8_b()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
#define GCU_TYPE8_B(val) ((GCU_Type8_Union) {.b = val})

/**
 * A union of all basic, 64-bit types to be used by generic, 64-bit containers.
 */
typedef union {
  void * p;          ///< Pointer type value.
  uint64_t ui64;     ///< uint64_t value.
  uint32_t ui32;     ///< uint32_t value.
  uint16_t ui16;     ///< uint16_t value.
  uint8_t ui8;       ///< uint8_t value.
  int64_t i64;       ///< int64_t value.
  int32_t i32;       ///< int32_t value.
  int16_t i16;       ///< int16_t value.
  int8_t i8;         ///< int8_t value.
  GCU_float64_t f64; ///< 64-bit float value.
  GCU_float32_t f32; ///< 32-bit float value.
  wchar_t wc;        ///< wchar_t value.
  char c;            ///< char value.
  bool b;            ///< bool value.
} GCU_Type64_Union;

/**
 * A union of all basic, 32-bit types to be used by generic, 32-bit containers.
 */
typedef union {
  uint32_t ui32;     ///< uint32_t value.
  uint16_t ui16;     ///< uint16_t value.
  uint8_t ui8;       ///< uint8_t value.
  int32_t i32;       ///< int32_t value.
  int16_t i16;       ///< int16_t value.
  int8_t i8;         ///< int8_t value.
  GCU_float32_t f32; ///< 32-bit float value.
#if GCU_WCHAR_WIDTH <= 4 || DOXYGEN
  wchar_t wc;        ///< wchar_t value.
#endif
  char c;            ///< char value.
  bool b;            ///< bool value.
} GCU_Type32_Union;

/**
 * A union of all basic, 16-bit types to be used by generic, 16-bit containers.
 */
typedef union {
  uint16_t ui16;     ///< uint16_t value.
  uint8_t ui8;       ///< uint8_t value.
  int16_t i16;       ///< int16_t value.
  int8_t i8;         ///< int8_t value.
  char c;            ///< char value.
  bool b;            ///< bool value.
} GCU_Type16_Union;

/**
 * A union of all basic, 8-bit types to be used by generic, 8-bit containers.
 */
typedef union {
  uint8_t ui8;       ///< uint8_t value.
  int8_t i8;         ///< int8_t value.
  char c;            ///< char value.
  bool b;            ///< bool value.
} GCU_Type8_Union;

/**
 * Create a 64-bit union variable with the type `void *`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_P()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_p(void * val);

/**
 * Create a 64-bit union variable with the type `uint64_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_UI64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_ui64(uint64_t val);

/**
 * Create a 64-bit union variable with the type `uint32_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_UI32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_ui32(uint32_t val);

/**
 * Create a 64-bit union variable with the type `uint16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_UI16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_ui16(uint16_t val);

/**
 * Create a 64-bit union variable with the type `uint8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_UI8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_ui8(uint8_t val);

/**
 * Create a 64-bit union variable with the type `int64_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_I64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_i64(int64_t val);

/**
 * Create a 64-bit union variable with the type `int32_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_I32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_i32(int32_t val);

/**
 * Create a 64-bit union variable with the type `int16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_I16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_i16(int16_t val);

/**
 * Create a 64-bit union variable with the type `int8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_I8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_i8(int8_t val);

/**
 * Create a 64-bit union variable with the type float with 64 bits.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_F64()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_f64(GCU_float64_t val);

/**
 * Create a 64-bit union variable with the type float with 32 bits.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_F32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_f32(GCU_float32_t val);

/**
 * Create a 64-bit union variable with the type `wchar_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_WC()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_wc(wchar_t val);

/**
 * Create a 64-bit union variable with the type `char`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_C()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_c(char val);

/**
 * Create a 64-bit union variable with the type `bool`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE64_B()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type64_Union gcu_type64_b(bool val);

/**
 * Create a 32-bit union variable with the type `uint32_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_UI32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_ui32(uint32_t val);

/**
 * Create a 32-bit union variable with the type `uint16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_UI16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_ui16(uint16_t val);

/**
 * Create a 32-bit union variable with the type `uint8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_UI8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_ui8(uint8_t val);

/**
 * Create a 32-bit union variable with the type `int32_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_I32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_i32(int32_t val);

/**
 * Create a 32-bit union variable with the type `int16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_I16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_i16(int16_t val);

/**
 * Create a 32-bit union variable with the type `int8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_I8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_i8(int8_t val);

/**
 * Create a 32-bit union variable with the type float with 32 bits.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_F32()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_f32(GCU_float32_t val);

#if GCU_WCHAR_WIDTH <= 4
/**
 * Create a 32-bit union variable with the type `wchar_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_WC()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_wc(wchar_t val);
#endif

/**
 * Create a 32-bit union variable with the type `char`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_C()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_c(char val);

/**
 * Create a 32-bit union variable with the type `bool`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE32_B()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type32_Union gcu_type32_b(bool val);

/**
 * Create a 16-bit union variable with the type `uint16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_UI16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_ui16(uint16_t val);

/**
 * Create a 16-bit union variable with the type `uint8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_UI8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_ui8(uint8_t val);

/**
 * Create a 16-bit union variable with the type `int16_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_I16()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_i16(int16_t val);

/**
 * Create a 16-bit union variable with the type `int8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_I8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_i8(int8_t val);

/**
 * Create a 16-bit union variable with the type `char`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_C()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_c(char val);

/**
 * Create a 16-bit union variable with the type `bool`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE16_B()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type16_Union gcu_type16_b(bool val);


/**
 * Create a 8-bit union variable with the type `uint8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE8_UI8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type8_Union gcu_type8_ui8(uint8_t val);

/**
 * Create a 8-bit union variable with the type `int8_t`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE8_I8()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type8_Union gcu_type8_i8(int8_t val);

/**
 * Create a 8-bit union variable with the type `char`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE8_C()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type8_Union gcu_type8_c(char val);

/**
 * Create a 8-bit union variable with the type `bool`.
 *
 * This function is provided as a helper in C++ because C++ does not allow the
 * use of compound literals.  If in C, use the `#define`.
 *
 * @see GCU_TYPE8_B()
 *
 * @param val The value to put into the union.
 * @return The union variable.
 */
GCU_Type8_Union gcu_type8_b(bool val);

#ifdef __cplusplus
}
#endif

#endif //GHOTIIO_CUTIL_TYPE_H

