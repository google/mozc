// Copyright 2010-2013, Google Inc.
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
#include "win32/ime/ime_trace.h"

namespace mozc {
namespace win32 {
namespace trace {
namespace {
void OutputMessage(const wchar_t *format, ...) {
  va_list arguments;
  va_start(arguments, format);

  wchar_t *message = nullptr;
  const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                        FORMAT_MESSAGE_FROM_STRING,
                                        format, 0, 0,
                                        reinterpret_cast<wchar_t*>(&message),
                                        0, &arguments);
  if (length > 0) {
    ::OutputDebugStringW(message);
  } else {
    ::OutputDebugStringW(L"FormatMessageW failed");
  }
  ::LocalFree(message);

  va_end(arguments);
}
}  // anonymous namespace

SimpleTracer::SimpleTracer(const wchar_t *function_name)
    : function_name_(function_name) {
#if !defined(DEBUG)
  if (!::IsDebuggerPresent()) {
     return;
  }
#endif  // !DEBUG
  OutputMessage(L"%1!s!: %2!s!;\n", function_name_, L"enter");
}

SimpleTracer::~SimpleTracer() {
#if !defined(DEBUG)
  if (!::IsDebuggerPresent()) {
     return;
  }
#endif  // !DEBUG
  OutputMessage(L"%1!s!: %2!s!;\n", function_name_, L"exit");
}

void SimpleTracer::Trace(const wchar_t *function_name, const int line,
                         const wchar_t *message) {
#if !defined(DEBUG)
  if (!::IsDebuggerPresent()) {
     return;
  }
#endif  // !DEBUG
  OutputMessage(L"%1!s!(%2!d!): %3!s!; %4!s!\n",
                function_name, line, L"trace", message);
}

void SimpleTracer::TraceFormat(const wchar_t *function_name, const int line,
                               const wchar_t *format, ...) {
#if !defined(DEBUG)
  if (!::IsDebuggerPresent()) {
     return;
  }
#endif  // !DEBUG
  va_list arguments;
  va_start(arguments, format);

  wchar_t *message = nullptr;
  const DWORD length = ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                        FORMAT_MESSAGE_FROM_STRING,
                                        format, 0, 0,
                                        reinterpret_cast<wchar_t*>(&message),
                                        0, &arguments);
  if (length > 0) {
    OutputMessage(L"%1!s!(%2!d!): %3!s!; %4!s!\n",
                  function_name, line, L"trace", message);
  } else {
    ::OutputDebugStringW(L"FormatMessageW failed");
  }
  ::LocalFree(message);

  va_end(arguments);
}
}  // namespace trace
}  // namespace win32
}  // namespace mozc
