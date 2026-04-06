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

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif  // __APPLE__

namespace mozc {
namespace port {
namespace internal {

// PlatformType represents a mutually exclusive list of target platforms.
enum class PlatformType {
  kWindows,   // Windows
  kLinux,     // Linux, excluding Android (different from target_is_linux)
  kMacos,     // macOS
  kAndroid,   // Android
  kIos,       // iOS Devices or Simulator
  kWasm,      // WebAssembly
  kChromeos,  // ChromeOS
};

// kTargetPlatform is the current build target platform.
#if defined(__linux__)
#if defined(__ANDROID__)
inline constexpr PlatformType kTargetPlatform = PlatformType::kAndroid;
#elif defined(OS_CHROMEOS)  // __ANDROID__
inline constexpr PlatformType kTargetPlatform = PlatformType::kChromeos;
#else                                        // OS_CHROMEOS
inline constexpr PlatformType kTargetPlatform = PlatformType::kLinux;
#endif                                       // !OS_CHROMEOS
#elif defined(_WIN32)                        // __linux__
inline constexpr PlatformType kTargetPlatform = PlatformType::kWindows;
#elif defined(__APPLE__)                     // _WIN32
#if TARGET_OS_OSX
inline constexpr PlatformType kTargetPlatform = PlatformType::kMacos;
#elif TARGET_OS_IPHONE  // TARGET_OS_OSX
inline constexpr PlatformType kTargetPlatform = PlatformType::kIos;
#else                   // TARGET_OS_IPHONE
#error "Unsupported Apple platform target."
#endif                   // !TARGET_OS_IPHONE
#elif defined(__wasm__)  // __APPLE__
inline constexpr PlatformType kTargetPlatform = PlatformType::kWasm;
#else                    // __wasm__
#error "Unsupported target platform."
#endif  // !__wasm__

}  // namespace internal

// The following port::Is functions are a modern alternative for ifdef macros.
// You can use standard C++ semantics like 'if constexpr' and 'std::conditional'
// to switch compiling code for different platforms. Unlike ifdef preprocessor
// directives, all codes are always evaluated and required to be well-formed,
// which works better with development tools, like clangd and clang-tidy.
//
// Limitations:
// The C++ compile-time expressions don't completely replace macros.
// Particularly:
//  - Preprocessor macros like #include directives
//  - All statements must be well-formed. For example,   you can't call
//  (undefined) Windows functions without ifdefs (or SFINAE).
//  - All statements are still evaluated and compiled (and discarded). Depending
//  on the size of the block, it may slow compiling.
//
// Examples:
// - Switching code with if constexpr. The non-taken branch will be discarded.
//
// int Func() {
//   if constexpr (port::IsWindows()) {
//     // Windows implementation.
//   } else {
//     // Other platforms.
//   }
// }
//
// - Defining a type alias based on platforms.
//
// using Type = std::conditional_t<port::IsWindows(), WindowsType, OtherType>;
//
// - Defining a constant with different values.
//
// constexpr absl::Duration kTimeout = port::IsIos() ?
//              absl::Milliseconds(100) : absl::Milliseconds(10);
//

// The build target is Windows.
constexpr bool IsWindows() {
  return internal::kTargetPlatform == internal::PlatformType::kWindows;
}

// The build target is Linux, including Android and ChromeOS.
constexpr bool IsLinuxBase() {
  return internal::kTargetPlatform == internal::PlatformType::kLinux ||
         internal::kTargetPlatform == internal::PlatformType::kAndroid ||
         internal::kTargetPlatform == internal::PlatformType::kChromeos;
}

// The build target is Linux, excluding Android and ChromeOS.
constexpr bool IsLinux() {
  return internal::kTargetPlatform == internal::PlatformType::kLinux;
}

// The build target is Android.
constexpr bool IsAndroid() {
  return internal::kTargetPlatform == internal::PlatformType::kAndroid;
}

// The build target is Apple based, like macOS and iOS.
constexpr bool IsAppleBase() {
  return internal::kTargetPlatform == internal::PlatformType::kMacos ||
         internal::kTargetPlatform == internal::PlatformType::kIos;
}

// The build target is macOS.
constexpr bool IsMacos() {
  return internal::kTargetPlatform == internal::PlatformType::kMacos;
}

// The build target is iOS firmware, devices, or simulator.
constexpr bool IsIos() {
  return internal::kTargetPlatform == internal::PlatformType::kIos;
}

// The build target is WASM.
constexpr bool IsWasm() {
  return internal::kTargetPlatform == internal::PlatformType::kWasm;
}

// The build target is ChromeOS.
constexpr bool IsChromeos() {
  return internal::kTargetPlatform == internal::PlatformType::kChromeos;
}

}  // namespace port

// MSVC uses a vendor-specific attribute for [[no_unique_address]].
// https://en.cppreference.com/w/cpp/language/attributes/no_unique_address
#ifdef _MSC_VER
#define MOZC_NO_UNIQUE_ADDRESS_ATTRIBUTE [[msvc::no_unique_address]]
#else  // _MSC_VER
#define MOZC_NO_UNIQUE_ADDRESS_ATTRIBUTE [[no_unique_address]]
#endif  // !_MSC_VER

}  // namespace mozc

#endif  // MOZC_BASE_PORT_H_
