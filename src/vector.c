/**
 */

#include <stdlib.h>
#include <string.h>
#include "cutil/vector.h"

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

