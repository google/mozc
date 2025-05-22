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

#ifndef MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_
#define MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

namespace mozc {
namespace win32 {
namespace tsf {

// This class provides a utility method to derive an ITfContext object that is
// expected to support surrounding text TSF APIs, or get a nullptr if it is
// supposed to be unavailable.
//
// This class can be used to get a supplimental ITfContext object that supports
// full text store operations when the target ITfContext is actually an EditText
// or RichEdit controls. This mechanism is called Transitory Extensions.
// https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-extensions-or-how-to-get-full-text-store-support-in-tsf-unaware-controls
// https://web.archive.org/web/20140518145404/http://blogs.msdn.com/b/tsfaware/archive/2007/05/21/transitory-extensions.aspx
//
// Another important use case of this class is to filter out mulfunctioning
// ITfContext objects by returning a nullpter when there is no way to get
// surrounding text through TSF APIs. This is because CUAS (Cicero Unaware
// Application Support) does not try to fully support surrounding text APIs
// through IMM32 APIs such as IMR_DOCUMENTFEED. Such an ITfContext object needs
// to be filtered out before trying to get surrounding text through TSF APIs.
class TipTransitoryExtension {
 public:
  TipTransitoryExtension() = delete;
  TipTransitoryExtension(const TipTransitoryExtension &) = delete;
  TipTransitoryExtension &operator=(const TipTransitoryExtension &) = delete;

  // Returns full-text-store context if available, otherwise returns |nullptr|.
  static wil::com_ptr_nothrow<ITfContext> AsFullContext(ITfContext *context);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_
