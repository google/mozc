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

#ifndef MOZC_WIN32_TIP_TIP_COMPARTMENT_UTIL_H_
#define MOZC_WIN32_TIP_TIP_COMPARTMENT_UTIL_H_

#include <msctf.h>
#include <wil/resource.h>
#include <windows.h>

#include "base/win32/hresult.h"
#include "base/win32/hresultor.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipCompartmentUtil {
 public:
  TipCompartmentUtil() = delete;
  TipCompartmentUtil(const TipCompartmentUtil &) = delete;
  TipCompartmentUtil &operator=(const TipCompartmentUtil &) = delete;

  // Stores |data| into the compartment specified by |compartment_guid| and
  // owned by |compartment_manager|.
  static HResult Set(ITfCompartmentMgr *compartment_manager,
                     const GUID &compartment_guid, TfClientId client_id,
                     wil::unique_variant data);
  static HResult Set(ITfThreadMgr *thread_manager, const GUID &compartment_guid,
                     TfClientId client_id, wil::unique_variant data);
  static HResult Set(ITfDocumentMgr *document_manager,
                     const GUID &compartment_guid, TfClientId client_id,
                     wil::unique_variant data);
  static HResult Set(ITfContext *context, const GUID &compartment_guid,
                     TfClientId client_id, wil::unique_variant data);

  // Returns the associated data from the compartment specified by
  // |compartment_guid| and owned by |compartment_manager| successfully.
  // Caveats: Returned variant is VT_EMPTY when the compartment has not store
  // any data yet.
  static HResultOr<wil::unique_variant> Get(
      ITfCompartmentMgr *compartment_manager, const GUID &compartment_guid);
  static HResultOr<wil::unique_variant> Get(ITfThreadMgr *thread_manager,
                                            const GUID &compartment_guid);
  static HResultOr<wil::unique_variant> Get(ITfDocumentMgr *document_manager,
                                            const GUID &compartment_guid);
  static HResultOr<wil::unique_variant> Get(ITfContext *context,
                                            const GUID &compartment_guid);

  // Returns true when |data| is retrieved from the compartment specified by
  // |compartment_guid| and owned by |compartment_manager| successfully.
  // Returns false otherwise.
  // When the compartment has not store any data yet, this function sets
  // |default_data| as default data and copies it into |data|.
  static HResultOr<wil::unique_variant> GetAndEnsureDataExists(
      ITfCompartmentMgr *compartment_manager, const GUID &compartment_guid,
      TfClientId client_id, wil::unique_variant default_data);
  static HResultOr<wil::unique_variant> GetAndEnsureDataExists(
      ITfThreadMgr *thread_manager, const GUID &compartment_guid,
      TfClientId client_id, wil::unique_variant default_data);
  static HResultOr<wil::unique_variant> GetAndEnsureDataExists(
      ITfDocumentMgr *document_manager, const GUID &compartment_guid,
      TfClientId client_id, wil::unique_variant default_data);
  static HResultOr<wil::unique_variant> GetAndEnsureDataExists(
      ITfContext *context, const GUID &compartment_guid, TfClientId client_id,
      wil::unique_variant default_data);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_COMPARTMENT_UTIL_H_
