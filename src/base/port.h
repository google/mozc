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

#ifdef _WIN32
#define MOZC_OS_DEFINED
#endif  // _WIN32

#ifdef __APPLE__
#define MOZC_OS_DEFINED
#endif  // __APPLE__

#ifdef __ANDROID__
#define MOZC_OS_DEFINED
#endif  // __ANDROID__

#ifdef OS_NACL
#error "We no longer support NaCl. Still need? Report to b/158959918 ASAP."
#endif  // OS_NACL

#ifdef __linux__
// TODO(matsuzakit): Remove following guard.
// Currently __linux__ and __ANDROID__ are defined at the same time.
#if !defined(__ANDROID__)
#define MOZC_OS_DEFINED
#endif  // !__ANDROID__
#endif  // __linux__

#ifdef OS_IOS
#define MOZC_OS_DEFINED
#endif  // OS_IOS

#ifdef __wasm__
#define MOZC_OS_DEFINED
#endif  // __wasm__

#ifndef MOZC_OS_DEFINED
#error "Unsupported OS."
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

#endif  // GOOGLE_JAPANESE_INPUT_BUILD


#endif  // MOZC_BASE_PORT_H_
