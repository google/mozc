// Copyright 2010-2018, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_BASE_PORT_H_
#define MOZC_BASE_PORT_H_

#include "base/port_string.h"

// Check duplicate OS_XXX definition.

#ifdef OS_WIN
#define MOZC_OS_DEFINED
#endif  // OS_WIN

#ifdef OS_MACOSX
#define MOZC_OS_DEFINED
#endif  // OS_MACOSX

#ifdef OS_ANDROID
#define MOZC_OS_DEFINED
#endif  // OS_ANDROID

#ifdef OS_NACL
#define MOZC_OS_DEFINED
#endif  // OS_NACL

#ifdef OS_LINUX
// TODO(matsuzakit): Remove following guard.
// Currently OS_LINUX and (OS_ANDROID or OS_NACL) are defined at the same time.
#if !defined(OS_ANDROID) && !defined(OS_NACL)
#define MOZC_OS_DEFINED
#endif  // !OS_ANDROID && !OS_NACL
#endif  // OS_LINUX


#ifndef MOZC_OS_DEFINED
#error "OS_XXX (e.g., OS_WIN) must be defined."
#endif  // !MOZC_OS_DEFINED

#undef MOZC_OS_DEFINED



#ifndef _MSC_VER
#if !defined(__STDC_FORMAT_MACROS)
#define __STDC_FORMAT_MACROS
#endif  // !__STDC_FORMAT_MACROS
#include <inttypes.h>
#endif  // _MSC_VER
#include <sys/types.h>
#include <stdint.h>
#include <cstddef>

// Integral types.
#ifdef OS_WIN
using int8 = __int8;
using int16 = __int16;
using int32 = __int32;
using int64 = __int64;
using uint8 = unsigned __int8;
using uint16 = unsigned __int16;
using uint32 = unsigned __int32;
using uint64 = unsigned __int64;
#else  // OS_WIN
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
#endif  // OS_WIN
using char32 = uint32;

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#ifndef _MSC_VER
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif  // !_MSC_VER

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL

// Print format strings for 64-bit integers.
#ifdef _MSC_VER
#define MOZC_PRId64 "I64d"
#define MOZC_PRIo64 "I64o"
#define MOZC_PRIu64 "I64u"
#define MOZC_PRIx64 "I64x"
#define MOZC_PRIX64 "I64X"
#else  // _MSC_VER
#define MOZC_PRId64 PRId64
#define MOZC_PRIo64 PRIo64
#define MOZC_PRIu64 PRIu64
#define MOZC_PRIx64 PRIx64
#define MOZC_PRIX64 PRIX64
#endif  // _MSC_VER

// INT_MIN, INT_MAX, UINT_MAX family at Google
static const uint8  kuint8max  = (( uint8) 0xFF);
static const uint16 kuint16max = ((uint16) 0xFFFF);
static const uint32 kuint32max = ((uint32) 0xFFFFFFFF);
static const uint64 kuint64max = ((uint64) GG_LONGLONG(0xFFFFFFFFFFFFFFFF));
static const  int8  kint8min   = ((  int8) 0x80);
static const  int8  kint8max   = ((  int8) 0x7F);
static const  int16 kint16min  = (( int16) 0x8000);
static const  int16 kint16max  = (( int16) 0x7FFF);
static const  int32 kint32min  = (( int32) 0x80000000);
static const  int32 kint32max  = (( int32) 0x7FFFFFFF);
static const  int64 kint64min  = (( int64) GG_LONGLONG(0x8000000000000000));
static const  int64 kint64max  = (( int64) GG_LONGLONG(0x7FFFFFFFFFFFFFFF));

#define DISALLOW_COPY_AND_ASSIGN(TypeName)    \
    TypeName(const TypeName&) = delete;       \
    void operator=(const TypeName&) = delete

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName)  \
    TypeName() = delete;                          \
    DISALLOW_COPY_AND_ASSIGN(TypeName)

// Macro for annotating implicit fall-through
// TODO(team): Implement this.
#define  FALLTHROUGH_INTENDED do { } while (0)

#if defined(__GNUC__) || defined(__clang__)
// Tell the compiler to do printf format string checking if the
// compiler supports it; see the 'format' attribute in
// <http://gcc.gnu.org/onlinedocs/gcc-4.3.0/gcc/Function-Attributes.html>.
//
// N.B.: As the GCC manual states, "[s]ince non-static C++ methods
// have an implicit 'this' argument, the arguments of such methods
// should be counted from two, not one."
#define ABSL_PRINTF_ATTRIBUTE(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#define ABSL_SCANF_ATTRIBUTE(string_index, first_to_check) \
  __attribute__((__format__(__scanf__, string_index, first_to_check)))
#else
#define ABSL_PRINTF_ATTRIBUTE(string_index, first_to_check)
#define ABSL_SCANF_ATTRIBUTE(string_index, first_to_check)
#endif  // __GNUC__ || __clang__

#define AS_STRING(x)   AS_STRING_INTERNAL(x)
#define AS_STRING_INTERNAL(x)   #x

#endif  // MOZC_BASE_PORT_H_
