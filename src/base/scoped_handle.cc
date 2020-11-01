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

#include "base/scoped_handle.h"

#ifdef OS_WIN
#include <windows.h>

namespace mozc {

ScopedHandle::ScopedHandle() : handle_(nullptr) {}

ScopedHandle::ScopedHandle(Win32Handle handle) : handle_(nullptr) {
  reset(handle);
}

ScopedHandle::~ScopedHandle() { Close(); }

void ScopedHandle::reset(Win32Handle handle) {
  Close();
  // Comment by yukawa:
  // FYI, ATL has similar class called CHandle and its document
  // says that we cannot
  // assume INVALID_HANDLE_VALUE is the only invalid handle.
  // See the note section of this document.
  // http://msdn.microsoft.com/en-us/library/5fc6ft2t.aspx
  // .NET approach is also informative.
  // .NET has similar handle wrapper called SafeHandle,
  // which allows us to override
  // the condition of invalid handle.
  // http://msdn.microsoft.com/en-us/library/system.runtime.interopservices.safehandle.isinvalid.aspx
  // They has pre-defined special classes SafeHandleMinusOneIsInvalid and
  // SafeHandleZeroOrMinusOneIsInvalid.
  // http://msdn.microsoft.com/en-us/library/microsoft.win32.safehandles.safehandleminusoneisinvalid.aspx
  // http://msdn.microsoft.com/en-us/library/microsoft.win32.safehandles.safehandlezeroorminusoneisinvalid.aspx
  if (handle != INVALID_HANDLE_VALUE) {
    handle_ = handle;
  }
}

ScopedHandle::Win32Handle ScopedHandle::get() const { return handle_; }

// transfers ownership away from this object
ScopedHandle::Win32Handle ScopedHandle::take() {
  HANDLE handle = handle_;
  handle_ = nullptr;
  return handle;
}

void ScopedHandle::Close() {
  if (handle_ != nullptr) {
    ::CloseHandle(handle_);
    handle_ = nullptr;
  }
}

}  // namespace mozc

#endif  // OS_WIN
