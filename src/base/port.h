// Copyright 2010-2011, Google Inc.
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


#include <sys/types.h>

// basic macros
typedef signed char         int8;
typedef short               int16;
typedef int                 int32;
typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned int       char32;
#ifdef OS_WINDOWS
typedef unsigned __int64   uint64;
typedef __int64             int64;
#else
typedef unsigned long long uint64;
typedef long long           int64;
#endif

#define atoi32 atoi
#define strto32 strtol
#define strto64 strtoll

#ifdef OS_WINDOWS
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>  // time()
#define snprintf _snprintf_s
#define strtoull _strtoui64
#define strtoll  _strtoi64

// va_copy portability definitions
#ifdef COMPILER_MSVC
// MSVC doesn't have va_copy yet.
// This is believed to work for 32-bit msvc.  This may not work at all for
// other platforms.
// If va_list uses the single-element-array trick, you will probably get
// a compiler error here.
#include <stdarg.h>
inline void va_copy(va_list& a, va_list& b) {
  a = b;
}
#endif
#endif

template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

#ifndef OS_WINDOWS
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

#define arraysize(array) (sizeof(ArraySizeHelper(array)))

// ARRAYSIZE performs essentially the same calculation as arraysize,
// but can be used on anonymous types or types defined inside
// functions.  It's less safe than arraysize as it accepts some
// (although not all) pointers.  Therefore, you should use arraysize
// whenever possible.
//
// Starting with Visual C++ 2005, WinNT.h includes ARRAYSIZE.
#if !defined(COMPILER_MSVC) || (defined(_MSC_VER) && _MSC_VER < 1400)
#define ARRAYSIZE(a) \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

#ifdef OS_WINDOWS
// I64/UI64 postfixes are equivalent to LL/ULL and "I64" is equivalent to
// "ll" since Visual C++ 2005, so we no longer need Windows-specific hack
// for these, but we keep using old hacks.  The reasons are
// - these hack support wider compilers including Visual C++ 2003 and prior.
//   (Despite the fact GG_LONGLONG and GG_ULONGLONG work well on Visual C++
//    2003, supporting Visual C++ 2003 is not a goal of Mozc.  Please test
//    your code with Visual C++ 2008 in terms of Mozc on Windows.)
// - despite the names of GG_LONGLONG and GG_LL_FORMAT, what we want are
//   for type int64, NOT for type long long.  So the name of I64/UI64 should
//   be much more appropriate and safer against future changes.
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64
// Length modifier in printf format string for int64's (e.g. within %d)
#define GG_LL_FORMAT "I64"  // As in printf("%I64d", ...)
#define GG_LL_FORMAT_W L"I64"
#else
#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL
#define GG_LL_FORMAT "ll"  // As in "%lld". Note that "q" is poor form also.
#define GG_LL_FORMAT_W L"ll"
#endif  // OS_WINDOWS

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

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#if (defined(COMPILER_GCC3) || defined(COMPILER_ICC) || defined(OS_MACOSX)) && !defined(SWIG)
// Tell the compiler to do printf format string checking if the
// compiler supports it; see the 'format' attribute in
// <http://gcc.gnu.org/onlinedocs/gcc-4.3.0/gcc/Function-Attributes.html>.
//
// N.B.: As the GCC manual states, "[s]ince non-static C++ methods
// have an implicit 'this' argument, the arguments of such methods
// should be counted from two, not one."
#define PRINTF_ATTRIBUTE(string_index, first_to_check) \
    __attribute__((__format__ (__printf__, string_index, first_to_check)))
#define SCANF_ATTRIBUTE(string_index, first_to_check) \
    __attribute__((__format__ (__scanf__, string_index, first_to_check)))
#else
#define PRINTF_ATTRIBUTE(string_index, first_to_check)
#define SCANF_ATTRIBUTE(string_index, first_to_check)
#endif

#ifndef SWIG
# define ABSTRACT = 0
#endif

template <bool>
struct CompileAssert {
};

#define COMPILE_ASSERT(expr, msg) \
  typedef CompileAssert<(bool(expr))> msg[bool(expr) ? 1 : -1]

#define AS_STRING(x)   AS_STRING_INTERNAL(x)
#define AS_STRING_INTERNAL(x)   #x

#include "base/logging.h"
#include "base/flags.h"


#include "base/init.h"
#include "base/flags.h"

#endif  // MOZC_BASE_PORT_H_
