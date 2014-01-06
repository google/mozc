// Copyright 2010-2014, Google Inc.
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

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <msctf.h>

#include "base/port.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipCompartmentUtil {
 public:
  // Returns true when |data| is stored into the compartment specified by
  // |compartment_guid| and owned by |compartment_manager| successfully.
  // Returns false otherwise.
  static bool Set(ITfCompartmentMgr *compartment_manager,
                  const GUID &compartment_guid,
                  TfClientId client_id,
                  const ATL::CComVariant &data);
  static bool Set(ITfThreadMgr *thread_manager,
                  const GUID &compartment_guid,
                  TfClientId client_id,
                  const ATL::CComVariant &data);
  static bool Set(ITfDocumentMgr *document_manager,
                  const GUID &compartment_guid,
                  TfClientId client_id,
                  const ATL::CComVariant &data);
  static bool Set(ITfContext *context,
                  const GUID &compartment_guid,
                  TfClientId client_id,
                  const ATL::CComVariant &data);

  // Returns true when |data| is retrieved from the compartment specified by
  // |compartment_guid| and owned by |compartment_manager| successfully.
  // Returns false otherwise.
  // Caveats: Returns true and |data| will be VT_EMPTY when the compartment
  //     has not store any data yet.
  static bool Get(ITfCompartmentMgr *compartment_manager,
                  const GUID &compartment_guid,
                  ATL::CComVariant *data);
  static bool Get(ITfThreadMgr *thread_manager,
                  const GUID &compartment_guid,
                  ATL::CComVariant *data);
  static bool Get(ITfDocumentMgr *document_manager,
                  const GUID &compartment_guid,
                  ATL::CComVariant *data);
  static bool Get(ITfContext *context,
                  const GUID &compartment_guid,
                  ATL::CComVariant *data);

  // Returns true when |data| is retrieved from the compartment specified by
  // |compartment_guid| and owned by |compartment_manager| successfully.
  // Returns false otherwise.
  // When the compartment has not store any data yet, this function sets
  // |default_data| as default data and copies it into |data|.
  static bool GetAndEnsureDataExists(
      ITfCompartmentMgr *compartment_manager,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data);
  static bool GetAndEnsureDataExists(
      ITfThreadMgr *thread_manager,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data);
  static bool GetAndEnsureDataExists(
      ITfDocumentMgr *document_manager,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data);
  static bool GetAndEnsureDataExists(
      ITfContext *context,
      const GUID &compartment_guid,
      TfClientId client_id,
      const ATL::CComVariant &default_data,
      ATL::CComVariant *data);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TipCompartmentUtil);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_COMPARTMENT_UTIL_H_
