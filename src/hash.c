/*
 * SPDX-License-Identifier: LGPL-3.0-only
 *
 * Copyright (C) 2023-2026 Corey Pennycuff
 *
 * This file is part of Ghoti.io CUtil.
 *
 * Ghoti.io CUtil is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Ghoti.io CUtil is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/hash.h>
#include <ghoti.io/cutil/memory.h>

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

