/**
 * @file
 */

#include "cutil/type.h"


GCU_Type_Union gcu_type_p(void * val) {
  return (GCU_Type_Union) {
    .p = val,
  };
}

GCU_Type_Union gcu_type_ui64(uint64_t val) {
  return (GCU_Type_Union) {
    .ui64 = val,
  };
}

GCU_Type_Union gcu_type_ui32(uint32_t val) {
  return (GCU_Type_Union) {
    .ui32 = val,
  };
}

GCU_Type_Union gcu_type_ui16(uint16_t val) {
  return (GCU_Type_Union) {
    .ui16 = val,
  };
}

GCU_Type_Union gcu_type_ui8(uint8_t val) {
  return (GCU_Type_Union) {
    .ui8 = val,
  };
}

GCU_Type_Union gcu_type_i64(int64_t val) {
  return (GCU_Type_Union) {
    .i64 = val,
  };
}

GCU_Type_Union gcu_type_i32(int32_t val) {
  return (GCU_Type_Union) {
    .i32 = val,
  };
}

GCU_Type_Union gcu_type_i16(int16_t val) {
  return (GCU_Type_Union) {
    .i16 = val,
  };
}

GCU_Type_Union gcu_type_i8(int8_t val) {
  return (GCU_Type_Union) {
    .i8 = val,
  };
}

GCU_Type_Union gcu_type_f64(GCU_float64_t val) {
  return (GCU_Type_Union) {
    .f64 = val,
  };
}

GCU_Type_Union gcu_type_f32(GCU_float32_t val) {
  return (GCU_Type_Union) {
    .f32 = val,
  };
}

GCU_Type_Union gcu_type_c(char val) {
  return (GCU_Type_Union) {
    .c = val,
  };
}

