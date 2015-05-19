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

// This header file intentionally does not have include guard because
// the purpose of this header file is embedding boilerplate code and
// you may want to use this header file multiple times from a file.

// This header file provides a generic way to save compiler's warning
// settings so that you can restore the settings later by
// "pop_warning_settings.h" when you want to disable some unavoidable
// warnings temporarily.
// Currently Visual C++, GCC 4.6 and later, and Clang are supported.
// On GCC 4.5 and prior, this header does nothing.
//
// Usage example:
//   #include "base/push_warning_settings.h"
//   #if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 405)
//   #pragma GCC diagnostic ignored "-Wconversion-null"
//   #endif  // GCC 4.5 and greater
//   EXPECT_EQ(false, false);
//   #include "base/pop_warning_settings.h"

#if defined(_MSC_VER)
  // Visual C++
  #pragma warning(push)
#elif defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 406)
  // G++ 4.6 and greater
  #pragma GCC diagnostic push
#elif defined(__clang__)
  // Clang
  #pragma clang diagnostic push
#endif
