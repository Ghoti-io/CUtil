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
 * @file
 * UTF-8 and UTF-16 conversion.  See utf.h for the contract, and for why this
 * file has no platform branches.
 */

#include <ghoti.io/cutil/macros.h>
#include <ghoti.io/cutil/utf.h>

// A code point, or this, which no valid input can produce.
#define GCU_UTF_INVALID 0xFFFFFFFFu

#define GCU_UTF_MAX          0x10FFFFu
#define GCU_UTF_SURROGATE_LO 0xD800u
#define GCU_UTF_SURROGATE_HI 0xDFFFu

//
// Decode one code point, advancing *index past it.
//
// The length checks are written as explicit lower bounds per length rather
// than as a "shortest form" test afterwards, because the bound is what makes
// an overlong encoding unrepresentable rather than merely detected: C0 80
// cannot reach 0x80, so it never becomes a NUL that a later strlen believes.
//
static uint32_t decode_utf8(const unsigned char * s, size_t * index) {
  uint32_t first = s[*index];

  if (first < 0x80u) {
    *index += 1;
    return first;
  }

  // A continuation byte cannot start a sequence, and 0xFE/0xFF never appear.
  if (first < 0xC2u || first > 0xF4u) {
    return GCU_UTF_INVALID;
  }

  size_t length;
  uint32_t code;
  if (first < 0xE0u) {
    length = 2;
    code = first & 0x1Fu;
  }
  else if (first < 0xF0u) {
    length = 3;
    code = first & 0x0Fu;
  }
  else {
    length = 4;
    code = first & 0x07u;
  }

  for (size_t i = 1; i < length; ++i) {
    unsigned char next = s[*index + i];
    // Reaching the terminator mid-sequence is a truncated string, and is
    // caught here because the NUL is not a continuation byte.
    if ((next & 0xC0u) != 0x80u) {
      return GCU_UTF_INVALID;
    }
    code = (code << 6) | (next & 0x3Fu);
  }

  // Shortest form: 0xC2 above already excludes the 2-byte overlongs, so only
  // the 3- and 4-byte cases need a bound here.
  if ((length == 3 && code < 0x800u) || (length == 4 && code < 0x10000u)) {
    return GCU_UTF_INVALID;
  }
  if (code > GCU_UTF_MAX) {
    return GCU_UTF_INVALID;
  }
  // A surrogate encoded in UTF-8 is not UTF-8.  Accepting it is what makes a
  // converter round-trip WTF-8 and hand a lone surrogate to an API that
  // cannot represent one.
  if (code >= GCU_UTF_SURROGATE_LO && code <= GCU_UTF_SURROGATE_HI) {
    return GCU_UTF_INVALID;
  }

  *index += length;
  return code;
}

// How many bytes this code point needs in UTF-8.  Caller has validated it.
static size_t utf8_length_of(uint32_t code) {
  if (code < 0x80u) {
    return 1;
  }
  if (code < 0x800u) {
    return 2;
  }
  if (code < 0x10000u) {
    return 3;
  }
  return 4;
}

static void encode_utf8(uint32_t code, char * out, size_t * index) {
  if (code < 0x80u) {
    out[(*index)++] = (char)code;
  }
  else if (code < 0x800u) {
    out[(*index)++] = (char)(0xC0u | (code >> 6));
    out[(*index)++] = (char)(0x80u | (code & 0x3Fu));
  }
  else if (code < 0x10000u) {
    out[(*index)++] = (char)(0xE0u | (code >> 12));
    out[(*index)++] = (char)(0x80u | ((code >> 6) & 0x3Fu));
    out[(*index)++] = (char)(0x80u | (code & 0x3Fu));
  }
  else {
    out[(*index)++] = (char)(0xF0u | (code >> 18));
    out[(*index)++] = (char)(0x80u | ((code >> 12) & 0x3Fu));
    out[(*index)++] = (char)(0x80u | ((code >> 6) & 0x3Fu));
    out[(*index)++] = (char)(0x80u | (code & 0x3Fu));
  }
}

bool gcu_utf8_is_valid(const char * utf8) {
  if (!utf8) {
    return false;
  }
  const unsigned char * s = (const unsigned char *)utf8;
  size_t i = 0;
  while (s[i]) {
    if (decode_utf8(s, &i) == GCU_UTF_INVALID) {
      return false;
    }
  }
  return true;
}

size_t gcu_utf8_to_utf16(const char * utf8, GCU_Char16 * out,
    size_t out_units) {
  if (!utf8) {
    return 0;
  }

  const unsigned char * s = (const unsigned char *)utf8;

  // Measure first, always, even when converting.  The alternative -- writing
  // as we go and stopping on an error -- leaves the caller's buffer holding a
  // prefix of a string that was never valid.
  size_t units = 0;
  size_t i = 0;
  while (s[i]) {
    uint32_t code = decode_utf8(s, &i);
    if (code == GCU_UTF_INVALID) {
      return 0;
    }
    units += code < 0x10000u ? 1 : 2;
  }
  ++units; // terminator

  if (!out || out_units < units) {
    return units;
  }

  size_t o = 0;
  i = 0;
  while (s[i]) {
    uint32_t code = decode_utf8(s, &i);
    if (code < 0x10000u) {
      out[o++] = (GCU_Char16)code;
    }
    else {
      code -= 0x10000u;
      out[o++] = (GCU_Char16)(GCU_UTF_SURROGATE_LO + (code >> 10));
      out[o++] = (GCU_Char16)(0xDC00u + (code & 0x3FFu));
    }
  }
  out[o] = 0;
  return units;
}

size_t gcu_utf16_to_utf8(const GCU_Char16 * utf16, char * out,
    size_t out_bytes) {
  if (!utf16) {
    return 0;
  }

  size_t bytes = 0;
  size_t i = 0;
  while (utf16[i]) {
    uint32_t unit = utf16[i];

    if (unit >= GCU_UTF_SURROGATE_LO && unit <= 0xDBFFu) {
      uint32_t low = utf16[i + 1];
      if (low < 0xDC00u || low > GCU_UTF_SURROGATE_HI) {
        return 0; // high surrogate not followed by a low one
      }
      bytes += 4;
      i += 2;
      continue;
    }
    if (unit >= 0xDC00u && unit <= GCU_UTF_SURROGATE_HI) {
      return 0; // low surrogate with no high one before it
    }

    bytes += utf8_length_of(unit);
    ++i;
  }
  ++bytes; // terminator

  if (!out || out_bytes < bytes) {
    return bytes;
  }

  size_t o = 0;
  i = 0;
  while (utf16[i]) {
    uint32_t unit = utf16[i];
    if (unit >= GCU_UTF_SURROGATE_LO && unit <= 0xDBFFu) {
      uint32_t low = utf16[i + 1];
      uint32_t code = 0x10000u + ((unit - GCU_UTF_SURROGATE_LO) << 10)
          + (low - 0xDC00u);
      encode_utf8(code, out, &o);
      i += 2;
    }
    else {
      encode_utf8(unit, out, &o);
      ++i;
    }
  }
  out[o] = '\0';
  return bytes;
}
