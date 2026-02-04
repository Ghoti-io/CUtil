/**
 */

#include <cutil/type.h>


GCU_Type64_Union gcu_type64_p(void * val) {
  return GCU_TYPE64_P(val);
}

GCU_Type64_Union gcu_type64_ui64(uint64_t val) {
  return GCU_TYPE64_UI64(val);
}

GCU_Type64_Union gcu_type64_ui32(uint32_t val) {
  return GCU_TYPE64_UI32(val);
}

GCU_Type64_Union gcu_type64_ui16(uint16_t val) {
  return GCU_TYPE64_UI16(val);
}

GCU_Type64_Union gcu_type64_ui8(uint8_t val) {
  return GCU_TYPE64_UI8(val);
}

GCU_Type64_Union gcu_type64_i64(int64_t val) {
  return GCU_TYPE64_I64(val);
}

GCU_Type64_Union gcu_type64_i32(int32_t val) {
  return GCU_TYPE64_I32(val);
}

GCU_Type64_Union gcu_type64_i16(int16_t val) {
  return GCU_TYPE64_I16(val);
}

GCU_Type64_Union gcu_type64_i8(int8_t val) {
  return GCU_TYPE64_I8(val);
}

GCU_Type64_Union gcu_type64_f64(GCU_float64_t val) {
  return GCU_TYPE64_F64(val);
}

GCU_Type64_Union gcu_type64_f32(GCU_float32_t val) {
  return GCU_TYPE64_F32(val);
}

GCU_Type64_Union gcu_type64_wc(wchar_t val) {
  return GCU_TYPE64_WC(val);
}

GCU_Type64_Union gcu_type64_c(char val) {
  return GCU_TYPE64_C(val);
}

GCU_Type64_Union gcu_type64_b(bool val) {
  return GCU_TYPE64_B(val);
}

GCU_Type32_Union gcu_type32_ui32(uint32_t val) {
  return GCU_TYPE32_UI32(val);
}

GCU_Type32_Union gcu_type32_ui16(uint16_t val) {
  return GCU_TYPE32_UI16(val);
}

GCU_Type32_Union gcu_type32_ui8(uint8_t val) {
  return GCU_TYPE32_UI8(val);
}

GCU_Type32_Union gcu_type32_i32(int32_t val) {
  return GCU_TYPE32_I32(val);
}

GCU_Type32_Union gcu_type32_i16(int16_t val) {
  return GCU_TYPE32_I16(val);
}

GCU_Type32_Union gcu_type32_i8(int8_t val) {
  return GCU_TYPE32_I8(val);
}

GCU_Type32_Union gcu_type32_f32(GCU_float32_t val) {
  return GCU_TYPE32_F32(val);
}

#if GCU_WCHAR_WIDTH <= 4
GCU_Type32_Union gcu_type32_wc(wchar_t val) {
  return GCU_TYPE32_WC(val);
}
#endif

GCU_Type32_Union gcu_type32_c(char val) {
  return GCU_TYPE32_C(val);
}

GCU_Type32_Union gcu_type32_b(bool val) {
  return GCU_TYPE32_B(val);
}

GCU_Type16_Union gcu_type16_ui16(uint16_t val) {
  return GCU_TYPE16_UI16(val);
}

GCU_Type16_Union gcu_type16_ui8(uint8_t val) {
  return GCU_TYPE16_UI8(val);
}

GCU_Type16_Union gcu_type16_i16(int16_t val) {
  return GCU_TYPE16_I16(val);
}

GCU_Type16_Union gcu_type16_i8(int8_t val) {
  return GCU_TYPE16_I8(val);
}

GCU_Type16_Union gcu_type16_c(char val) {
  return GCU_TYPE16_C(val);
}

GCU_Type16_Union gcu_type16_b(bool val) {
  return GCU_TYPE16_B(val);
}

GCU_Type8_Union gcu_type8_ui8(uint8_t val) {
  return GCU_TYPE8_UI8(val);
}

GCU_Type8_Union gcu_type8_i8(int8_t val) {
  return GCU_TYPE8_I8(val);
}

GCU_Type8_Union gcu_type8_c(char val) {
  return GCU_TYPE8_C(val);
}

GCU_Type8_Union gcu_type8_b(bool val) {
  return GCU_TYPE8_B(val);
}

