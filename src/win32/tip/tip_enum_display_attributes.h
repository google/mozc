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

#ifndef MOZC_WIN32_TIP_TIP_ENUM_DISPLAY_ATTRIBUTES_H_
#define MOZC_WIN32_TIP_TIP_ENUM_DISPLAY_ATTRIBUTES_H_

#include <msctf.h>
#include <rpcsal.h>
#include <windows.h>

#include "absl/base/nullability.h"
#include "win32/tip/tip_dll_module.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Represents the list of the display attributes used in this module.
class TipEnumDisplayAttributes
    : public TipComImplements<IEnumTfDisplayAttributeInfo> {
 public:
  TipEnumDisplayAttributes() : index_(0) {}

  // IEnumTfDisplayAttributeInfo
  STDMETHODIMP
  Clone(IEnumTfDisplayAttributeInfo **absl_nullable enum_attributes) override;
  STDMETHODIMP
  Next(ULONG count, ITfDisplayAttributeInfo **absl_nonnull attribute_array,
       ULONG *absl_nullable fetched) override;
  STDMETHODIMP Reset() override;
  STDMETHODIMP Skip(ULONG count) override;

 private:
  LONG index_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_ENUM_DISPLAY_ATTRIBUTES_H_
