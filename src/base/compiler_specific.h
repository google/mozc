// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_BASE_COMPILER_SPECIFIC_H
#define MOZC_BASE_COMPILER_SPECIFIC_H

// TODO(yukawa): Add unit tests against macros below.
// TODO(yukawa): Support C++11's override/final keywords

// === Begin version check macro definitions ===
#if defined(_MSC_VER)
#define MOZC_MSVC_VERSION_GE(major, minor) (_MSC_VER >= major * 100 + minor)
#define MOZC_MSVC_VERSION_LE(major, minor) (_MSC_VER <= major * 100 + minor)
#else   // not _MSC_VER
#define MOZC_MSVC_VERSION_GE(major, minor) (0)
#define MOZC_MSVC_VERSION_LE(major, minor) (0)
#endif  // _MSC_VER

#if defined(__GNUC__)
#define MOZC_GCC_VERSION_GE(major, minor)                    \
    (__GNUC__ > (major) ||                                   \
     (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor)))
#define MOZC_GCC_VERSION_LE(major, minor)                    \
    (__GNUC__ < (major) ||                                   \
     (__GNUC__ == (major) && __GNUC_MINOR__ <= (minor)))
#else   // not __GNUC__
#define MOZC_GCC_VERSION_GE(major, minor) (0)
#define MOZC_GCC_VERSION_LE(major, minor) (0)
#endif  // __GNUC__

#if defined(__clang_major__)
#define MOZC_CLANG_VERSION_GE(major, minor)                  \
    (__clang_major__ > (major) ||                            \
     (__clang_major__ == (major) && __clang_minor__ >= (minor)))
#define MOZC_CLANG_VERSION_LE(major, minor)                  \
    (__clang_major__ < (major) ||                            \
     (__clang_major__ == (major) && __clang_minor__ <= (minor)))
#else  // not __clang_major__
#define MOZC_CLANG_VERSION_GE(major, minor) (0)
#define MOZC_CLANG_VERSION_LE(major, minor) (0)
#endif  // __clang_major__
// === End version check macro definitions ===

// === Begin inline pragma macro definitions ===
namespace mozc {
namespace compiler_specific_internal {
class SemicolonEater {};
}  // namespace compiler_specific_internal
}  // namespace mozc
#define MOZC_SWALLOWING_SEMICOLON_HACK              \
   using mozc::compiler_specific_internal::SemicolonEater
#if defined(_MSC_VER)
#define MOZC_DO_PRAGMA_IMPL(s) __pragma(s)  \
    MOZC_SWALLOWING_SEMICOLON_HACK
#elif defined(__clang__) || defined(__GNUC__)
#define MOZC_DO_PRAGMA_IMPL(s) _Pragma(#s)  \
    MOZC_SWALLOWING_SEMICOLON_HACK
#else
#define MOZC_DO_PRAGMA_IMPL(s)              \
    MOZC_SWALLOWING_SEMICOLON_HACK
#endif
// === End inline pragma macro definitions ===


// === Begin warning control macro definitions ===
// MOZC_MSVC_DISABLE_WARNING(n)
//   description:
//     Disables given warning.
//     Does nothing when not supported.
//
// MOZC_MSVC_PUSH_WARNING()
//   description:
//     Pushes current warning settings into the stack.
//     Does nothing when not supported.
//
// MOZC_MSVC_POP_WARNING()
//   description:
//     Pops the last warning settings from the stack.
//     Does nothing when not supported.
//   example:
//     MOZC_MSVC_PUSH_WARNING();
//     // Disable C4244
//     // http://msdn.microsoft.com/en-us/library/2d7604yb.aspx
//     MOZC_MSVC_DISABLE_WARNING(4001);
//     int x = 10.1;
//     MOZC_MSVC_POP_WARNING();
//
// MOZC_GCC_DISABLE_WARNING_FILELEVEL(x)
//   description:
//     Disables given warning. You can use this macro only on toplevel.
//     You cannot specify two or more tokens.
//     Does nothing when not supported.
//   example:
//     MOZC_GCC_DISABLE_WARNING_FILELEVEL(conversion-null);
//     TEST(Foo, Bar) {
//       EXPECT_EQ(false, false);
//     }
//
// MOZC_GCC_DISABLE_WARNING_INLINE(x)
//   description:
//     Disables given warning. You can use this macro inside of function.
//     You cannot specify two or more tokens.
//     Does nothing when not supported.
//   example:
//     TEST(Foo, Bar) {
//       MOZC_GCC_DISABLE_WARNING_INLINE(conversion-null);
//       EXPECT_EQ(false, false);
//     }
//
// MOZC_GCC_POP_WARNING -> GCC version of MOZC_MSVC_POP_WARNING
// MOZC_GCC_PUSH_WARNING -> GCC version of MOZC_MSVC_PUSH_WARNING
// MOZC_CLANG_POP_WARNING -> Clang version of MOZC_MSVC_POP_WARNING
// MOZC_CLANG_PUSH_WARNING -> Clang version of MOZC_MSVC_PUSH_WARNING
// MOZC_CLANG_DISABLE_WARNING
//     -> Clang version of MOZC_GCC_DISABLE_WARNING_INLINE

#if defined(_MSC_VER)
#define MOZC_MSVC_DISABLE_WARNING(n) MOZC_DO_PRAGMA_IMPL(warning(disable:n))
#define MOZC_MSVC_PUSH_WARNING() MOZC_DO_PRAGMA_IMPL(warning(push))
#define MOZC_MSVC_POP_WARNING() MOZC_DO_PRAGMA_IMPL(warning(pop))
#else  // not _MSC_VER
#define MOZC_MSVC_DISABLE_WARNING(n) MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_MSVC_PUSH_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_MSVC_POP_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#endif  // _MSC_VER

// Utility macros
#define MOZC_CLANG_GCC_DISABLE_WARING_IMPL(compiler, s)  \
    MOZC_DO_PRAGMA_IMPL(compiler diagnostic ignored #s)

#if MOZC_GCC_VERSION_GE(4, 2)
#define MOZC_GCC_DISABLE_WARNING_FILELEVEL(type)         \
    MOZC_CLANG_GCC_DISABLE_WARING_IMPL(GCC, -W ## type)
#else  // GCC<4.2
#define MOZC_GCC_DISABLE_WARNING_FILELEVEL(type)         \
    MOZC_SWALLOWING_SEMICOLON_HACK
#endif  // GCC versions

#if MOZC_GCC_VERSION_GE(4, 6)
#define MOZC_GCC_DISABLE_WARNING_INLINE(type)            \
    MOZC_CLANG_GCC_DISABLE_WARING_IMPL(GCC, -W ## type)
#define MOZC_GCC_POP_WARNING() MOZC_DO_PRAGMA_IMPL(GCC diagnostic pop)
#define MOZC_GCC_PUSH_WARNING() MOZC_DO_PRAGMA_IMPL(GCC diagnostic pop)
#else  // GCC<4.6
#define MOZC_GCC_DISABLE_WARNING_INLINE(type) MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_GCC_POP_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_GCC_PUSH_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#endif  // GCC versions

#if defined(__clang__)
#define MOZC_CLANG_DISABLE_WARNING(type)                 \
    MOZC_CLANG_GCC_DISABLE_WARING_IMPL(clang, -W ## type)
#define MOZC_CLANG_POP_WARNING() MOZC_DO_PRAGMA_IMPL(clang diagnostic pop)
#define MOZC_CLANG_PUSH_WARNING() MOZC_DO_PRAGMA_IMPL(clang diagnostic push)
#else  // !__clang__
#define MOZC_CLANG_DISABLE_WARNING(type) MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_CLANG_POP_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#define MOZC_CLANG_PUSH_WARNING() MOZC_SWALLOWING_SEMICOLON_HACK
#endif  // __clang__ or !__clang__
// === End waring control macro definitions ===

// === Begin compile message macro definitions ===
// MOZC_COMPILE_MESSAGE(msg)
//   description:
//     Generates a message into build log.
//     Does nothing when not supported.
//   example:
//     MOZC_COMPILE_MESSAGE("Hello");

// Utility macros
#if defined(_MSC_VER)
#define MOZC_COMPILE_MESSAGE(s) MOZC_DO_PRAGMA_IMPL(message (s))
#elif MOZC_GCC_VERSION_GE(4, 4) || MOZC_CLANG_VERSION_GE(2, 8)
#define MOZC_COMPILE_MESSAGE(s) MOZC_DO_PRAGMA_IMPL(message s)
#else
#define MOZC_COMPILE_MESSAGE(s) MOZC_SWALLOWING_SEMICOLON_HACK
#endif  // compilers
// === End compile message macro definitions ===

#endif  // MOZC_BASE_COMPILER_SPECIFIC_H
