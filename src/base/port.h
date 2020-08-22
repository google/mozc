// Copyright 2010-2020, Google Inc.
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

// TODO(komatsu): Remove this string include and using.
#include <string>

using std::string;

// Check duplicate OS_XXX definition.

#ifdef OS_WIN
#define MOZC_OS_DEFINED
#endif  // OS_WIN

#ifdef __APPLE__
#define MOZC_OS_DEFINED
#endif  // __APPLE__

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

#ifdef OS_IOS
#define MOZC_OS_DEFINED
#endif  // OS_IOS

#ifdef OS_WASM
#define MOZC_OS_DEFINED
#endif  // OS_WASM

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
#include <stdint.h>
#include <sys/types.h>
#include <cstddef>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#include "absl/base/integral_types.h"
#else  // GOOGLE_JAPANESE_INPUT_BUILD

// Integral types.
typedef signed char int8;
typedef short int16;  // NOLINT
typedef int int32;
#ifdef COMPILER_MSVC
typedef __int64 int64;
#else
typedef long long int64;  // NOLINT
#endif /* COMPILER_MSVC */

typedef unsigned char uint8;
typedef unsigned short uint16;  // NOLINT
typedef unsigned int uint32;
#ifdef COMPILER_MSVC
typedef unsigned __int64 uint64;
#else
typedef unsigned long long uint64;  // NOLINT
#endif /* COMPILER_MSVC */

typedef signed int char32;

#ifdef COMPILER_MSVC /* if Visual C++ */

// VC++ long long suffixes
#define GG_LONGLONG(x) x##I64
#define GG_ULONGLONG(x) x##UI64

#else /* not Visual C++ */

#define GG_LONGLONG(x) x##LL
#define GG_ULONGLONG(x) x##ULL

#endif  // COMPILER_MSVC

// INT_MIN, INT_MAX, UINT_MAX family at Google
const uint8 kuint8max{0xFF};
const uint16 kuint16max{0xFFFF};
const uint32 kuint32max{0xFFFFFFFF};
const uint64 kuint64max{GG_ULONGLONG(0xFFFFFFFFFFFFFFFF)};
const int8 kint8min{~0x7F};
const int8 kint8max{0x7F};
const int16 kint16min{~0x7FFF};
const int16 kint16max{0x7FFF};
const int32 kint32min{~0x7FFFFFFF};
const int32 kint32max{0x7FFFFFFF};
const int64 kint64min{GG_LONGLONG(~0x7FFFFFFFFFFFFFFF)};
const int64 kint64max{GG_LONGLONG(0x7FFFFFFFFFFFFFFF)};

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#ifndef arraysize
#define arraysize(array) ABSL_ARRAYSIZE(array)
#endif  // arraysize

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete

#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

#define AS_STRING(x) AS_STRING_INTERNAL(x)
#define AS_STRING_INTERNAL(x) #x


#endif  // MOZC_BASE_PORT_H_
