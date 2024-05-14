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

#include "base/port.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif  // __APPLE__

namespace mozc {
namespace {

#if defined(_WIN32)
static_assert(TargetIsWindows());
static_assert(!TargetIsLinux());
static_assert(!TargetIsAndroid());
static_assert(!TargetIsDarwin());
static_assert(!TargetIsOSX());
static_assert(!TargetIsIPhone());
static_assert(!TargetIsWASM());
static_assert(!TargetIsChromeOS());
#endif  // _WIN32

#if defined(__APPLE__)
static_assert(!TargetIsWindows());
static_assert(!TargetIsLinux());
static_assert(!TargetIsAndroid());
static_assert(TargetIsDarwin());
static_assert(!TargetIsWASM());
static_assert(!TargetIsChromeOS());
#if TARGET_OS_OSX
static_assert(TargetIsOSX());
static_assert(!TargetIsIPhone());
#elif TARGET_OS_IPHONE
static_assert(!TargetIsOSX());
static_assert(TargetIsIPhone());
#endif  // TARGET_OS_IPHONE
#endif  // __APPLE__

#if defined(__linux__)
static_assert(!TargetIsWindows());
static_assert(TargetIsLinux());
static_assert(!TargetIsDarwin());
static_assert(!TargetIsOSX());
static_assert(!TargetIsIPhone());
static_assert(!TargetIsWASM());
#if defined(__ANDROID__)
static_assert(TargetIsAndroid());
#else   // __ANDROID__
static_assert(!TargetIsAndroid());
#endif  // !__ANDROID__
#endif  // !__linux__

#ifdef OS_CHROMEOS
static_assert(TargetIsLinux());
static_assert(TargetIsChromeOS());
#else   // OS_CHROMEOS
static_assert(!TargetIsChromeOS());
#endif  // !OS_CHROMEOS

#if defined(__wasm__)
static_assert(!TargetIsWindows());
static_assert(!TargetIsLinux());
static_assert(!TargetIsAndroid());
static_assert(!TargetIsDarwin());
static_assert(!TargetIsOSX());
static_assert(!TargetIsIPhone());
static_assert(TargetIsWASM());
static_assert(!TargetIsChromeOS());
#endif  // __wasm__

}  // namespace
}  // namespace mozc
