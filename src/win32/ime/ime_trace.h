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

// A tiny trace utilities.

#ifndef MOZC_WIN32_IME_IME_TRACE_H
#define MOZC_WIN32_IME_IME_TRACE_H

#include <windows.h>
#include "base/port.h"

namespace mozc {
namespace win32 {
namespace trace {
// Simple logging class to avoid dependency to the Mozc base library.
// TODO(yukawa): Make a generic tracer class and replace this class.
class SimpleTracer {
 public:
  // You should not pass any temporal pointer to the |function_name|.
  // The pointer will be stored and used later.
  explicit SimpleTracer(const wchar_t *function_name);
  ~SimpleTracer();
  static void Trace(const wchar_t *function_name, const int line,
                    const wchar_t *message);
  static void TraceFormat(const wchar_t *function_name, const int line,
                          const wchar_t *format, ...);
 private:
  const wchar_t *function_name_;
  DISALLOW_COPY_AND_ASSIGN(SimpleTracer);
};
}  // namespace trace
}  // namespace win32
}  // namespace mozc

// You can put FUNCTION_ENTER(); at the front of the function
// to trace the timing of entering and exiting.
// Then you can use FUNCTION_TRACE to display a debug message.
// Then you can use FUNCTION_TRACE_FORMAT to a display debug message by
// FormatMessage format.
#if defined(NO_LOGGING)
#define FUNCTION_ENTER()
#define FUNCTION_TRACE(message)
#define FUNCTION_TRACE_FORMAT(format, ...)
#else
#define FUNCTION_ENTER() \
  mozc::win32::trace::SimpleTracer auto_tracer_object(TEXT(__FUNCTION__))
#define FUNCTION_TRACE(message) \
  mozc::win32::trace::SimpleTracer::Trace( \
      TEXT(__FUNCTION__), __LINE__, (message))
#define FUNCTION_TRACE_FORMAT(format, ...)  \
  mozc::win32::trace::SimpleTracer::TraceFormat( \
      TEXT(__FUNCTION__), __LINE__, (format), __VA_ARGS__)
#endif  // NO_LOGGING

#endif  // MOZC_WIN32_IME_IME_TRACE_H
