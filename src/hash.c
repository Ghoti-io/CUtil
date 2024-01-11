/**
 */

#include <stdbool.h>
#include <stdlib.h>
#include "cutil/hash.h"
#include "cutil/memory.h"

#define GROWTH_FACTOR 1.25

#define BITDEPTH 64
#define DEFAULT_TYPE gcu_type64_ui64
#include "hash.template.c"
#undef BITDEPTH
#undef DEFAULT_TYPE

#define BITDEPTH 32
#define DEFAULT_TYPE gcu_type32_ui32
#include "hash.template.c"
#undef BITDEPTH
#undef DEFAULT_TYPE

#define BITDEPTH 16
#define DEFAULT_TYPE gcu_type16_ui16
#include "hash.template.c"
#undef BITDEPTH
#undef DEFAULT_TYPE

#define BITDEPTH 8
#define DEFAULT_TYPE gcu_type8_ui8
#include "hash.template.c"
#undef BITDEPTH
#undef DEFAULT_TYPE

