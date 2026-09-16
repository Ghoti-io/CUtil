/**
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/memory.h>
#include <ghoti.io/cutil/vector.h>

#define GROWTH_FACTOR 1.3

#define BITDEPTH 64
#include "vector.template.c"
#undef BITDEPTH

#define BITDEPTH 32
#include "vector.template.c"
#undef BITDEPTH

#define BITDEPTH 16
#include "vector.template.c"
#undef BITDEPTH

#define BITDEPTH 8
#include "vector.template.c"
#undef BITDEPTH

