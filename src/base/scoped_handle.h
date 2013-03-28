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

#ifndef MOZC_BASE_SCOPED_HANDLE_H_
#define MOZC_BASE_SCOPED_HANDLE_H_

#ifdef OS_WIN
// Example:
//   ScopedHandle hfile(CreateFile(...));
//   if (!hfile.get())
//     ...process error
//   ReadFile(hfile.get(), ...);
namespace mozc {

class ScopedHandle {
 public:
  // In order not to depend on <Windows.h> from this header, here we
  // assume HANDLE type is a synonym of void *.
  typedef void *Win32Handle;

  // Initializes with NULL.
  ScopedHandle();

  // Initializes with taking ownership of |handle|.
  // Covert: If |handle| is INVALID_HANDLE_VALUE, this wrapper treat
  //     it as NULL.
  explicit ScopedHandle(Win32Handle handle);

  // Call ::CloseHandle API against the current object (if any).
  ~ScopedHandle();

  // Call ::CloseHandle API against the current object (if any), then
  // takes ownership of |handle|
  void reset(Win32Handle handle);

  // Returns the object pointer without transferring the ownership.
  Win32Handle get() const;

  // Transfers ownership away from this object.
  Win32Handle take();

 private:
  void Close();

  Win32Handle handle_;
};

}  // namespace mozc

#endif  // OS_WIN
#endif  // MOZC_BASE_SCOPED_HANDLE_H_
