/**
 * @file
 */

#include "cutil/type.h"


GCU_Type_Union gcu_type_ui32(uint32_t val) {
  return (GCU_Type_Union) {
    .ui32 = val,
  };
}

