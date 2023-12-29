/**
 * @file
 */

#include "cutil/type.h"


GCU_Type64_Union gcu_type64_p(void * val) {
  return (GCU_Type64_Union) {
    .p = val,
  };
}

GCU_Type64_Union gcu_type64_ui64(uint64_t val) {
  return (GCU_Type64_Union) {
    .ui64 = val,
  };
}

GCU_Type64_Union gcu_type64_ui32(uint32_t val) {
  return (GCU_Type64_Union) {
    .ui32 = val,
  };
}

GCU_Type64_Union gcu_type64_ui16(uint16_t val) {
  return (GCU_Type64_Union) {
    .ui16 = val,
  };
}

GCU_Type64_Union gcu_type64_ui8(uint8_t val) {
  return (GCU_Type64_Union) {
    .ui8 = val,
  };
}

GCU_Type64_Union gcu_type64_i64(int64_t val) {
  return (GCU_Type64_Union) {
    .i64 = val,
  };
}

GCU_Type64_Union gcu_type64_i32(int32_t val) {
  return (GCU_Type64_Union) {
    .i32 = val,
  };
}

GCU_Type64_Union gcu_type64_i16(int16_t val) {
  return (GCU_Type64_Union) {
    .i16 = val,
  };
}

GCU_Type64_Union gcu_type64_i8(int8_t val) {
  return (GCU_Type64_Union) {
    .i8 = val,
  };
}

GCU_Type64_Union gcu_type64_f64(GCU_float64_t val) {
  return (GCU_Type64_Union) {
    .f64 = val,
  };
}

GCU_Type64_Union gcu_type64_f32(GCU_float32_t val) {
  return (GCU_Type64_Union) {
    .f32 = val,
  };
}

GCU_Type64_Union gcu_type64_c(char val) {
  return (GCU_Type64_Union) {
    .c = val,
  };
}

GCU_Type32_Union gcu_type32_ui32(uint32_t val) {
  return (GCU_Type32_Union) {
    .ui32 = val,
  };
}

GCU_Type32_Union gcu_type32_ui16(uint16_t val) {
  return (GCU_Type32_Union) {
    .ui16 = val,
  };
}

GCU_Type32_Union gcu_type32_ui8(uint8_t val) {
  return (GCU_Type32_Union) {
    .ui8 = val,
  };
}

GCU_Type32_Union gcu_type32_i32(int32_t val) {
  return (GCU_Type32_Union) {
    .i32 = val,
  };
}

GCU_Type32_Union gcu_type32_i16(int16_t val) {
  return (GCU_Type32_Union) {
    .i16 = val,
  };
}

GCU_Type32_Union gcu_type32_i8(int8_t val) {
  return (GCU_Type32_Union) {
    .i8 = val,
  };
}

GCU_Type32_Union gcu_type32_f32(GCU_float32_t val) {
  return (GCU_Type32_Union) {
    .f32 = val,
  };
}

GCU_Type32_Union gcu_type32_c(char val) {
  return (GCU_Type32_Union) {
    .c = val,
  };
}

GCU_Type16_Union gcu_type16_ui16(uint16_t val) {
  return (GCU_Type16_Union) {
    .ui16 = val,
  };
}

GCU_Type16_Union gcu_type16_ui8(uint8_t val) {
  return (GCU_Type16_Union) {
    .ui8 = val,
  };
}

GCU_Type16_Union gcu_type16_i16(int16_t val) {
  return (GCU_Type16_Union) {
    .i16 = val,
  };
}

GCU_Type16_Union gcu_type16_i8(int8_t val) {
  return (GCU_Type16_Union) {
    .i8 = val,
  };
}

GCU_Type16_Union gcu_type16_c(char val) {
  return (GCU_Type16_Union) {
    .c = val,
  };
}

GCU_Type8_Union gcu_type8_ui8(uint8_t val) {
  return (GCU_Type8_Union) {
    .ui8 = val,
  };
}

GCU_Type8_Union gcu_type8_i8(int8_t val) {
  return (GCU_Type8_Union) {
    .i8 = val,
  };
}

GCU_Type8_Union gcu_type8_c(char val) {
  return (GCU_Type8_Union) {
    .c = val,
  };
}

