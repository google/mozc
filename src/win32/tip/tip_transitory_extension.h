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

#ifndef MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_
#define MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>
#include <msctf.h>

#include "base/port.h"

namespace mozc {
namespace win32 {
namespace tsf {

// This class provides utility methods to access parent object provided by
// Transitory Extensions.
// Starting with Windows Vista, CUAS provides Transitory Extensions with which
// full text store support is available for edit control, rich edit control,
// and Trident edit control. You can use these text stores mainly for one-shot
// read/read-write access. For keyboard input, which may require text
// composition, should use the original (transitory) text store.
// See the following article for the details about Transitory Extensions.
// http://blogs.msdn.com/b/tsfaware/archive/2007/05/21/transitory-extensions.aspx
class TipTransitoryExtension {
 public:
  // Returns the parent (full-text-store) document manager if exists.
  // Returns |document_manager| otherwise.
  static ATL::CComPtr<ITfDocumentMgr> ToParentDocumentIfExists(
      ITfDocumentMgr *document_manager);

  // Returns the parent (full-text-store) context if exists.
  // Returns |context| otherwise.
  static ATL::CComPtr<ITfContext> ToParentContextIfExists(ITfContext *context);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TipTransitoryExtension);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_TRANSITORY_EXTENSION_H_
