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
 * Simple program to generate correct floating point type names for a given
 * byte size.
 */

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if (argc == 2) {
    int bytes = atoi(argv[1]) / 8;
    if (sizeof(long double) == bytes) {
      printf("long double");
    }
    else if (sizeof(double) == bytes) {
      printf("double");
    }
    else if (sizeof(float) == bytes) {
      printf("float");
    }
  }
}

