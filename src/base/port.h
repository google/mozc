// Copyright 2010-2021, Google Inc.
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
#error "We no longer support NaCl. Still need? Report to b/158959918 ASAP."
#endif  // OS_NACL

#ifdef OS_LINUX
// TODO(matsuzakit): Remove following guard.
// Currently OS_LINUX and OS_ANDROID are defined at the same time.
#if !defined(OS_ANDROID)
#define MOZC_OS_DEFINED
#endif  // !OS_ANDROID
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



#ifdef GOOGLE_JAPANESE_INPUT_BUILD

#include "absl/base/attributes.h"
#include "absl/base/integral_types.h"
#include "absl/base/macros.h"

#else  // GOOGLE_JAPANESE_INPUT_BUILD

#include <cstdint>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"

// Integral types.
typedef std::int8_t int8;
typedef std::int16_t int16;
typedef std::int32_t int32;
typedef std::int64_t int64;

typedef std::uint8_t uint8;
typedef std::uint16_t uint16;
typedef std::uint32_t uint32;
typedef std::uint64_t uint64;

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
