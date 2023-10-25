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

#include <combaseapi.h>
#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include "testing/gunit.h"

namespace {

void AssertComIsNotInitialized() {
  APTTYPE type = APTTYPE_CURRENT;
  APTTYPEQUALIFIER filter = APTTYPEQUALIFIER_NONE;
  HRESULT result = ::CoGetApartmentType(&type, &filter);
  EXPECT_EQ(result, CO_E_NOTINITIALIZED);
}

TEST(MsctfDllTest, CreateITfCategoryMgr) {
  AssertComIsNotInitialized();

  wil::com_ptr_nothrow<ITfCategoryMgr> obj;
  HRESULT result = TF_CreateCategoryMgr(&obj);
  EXPECT_EQ(result, S_OK);
}

TEST(MsctfDllTest, CreateInputProcessorProfiles) {
  AssertComIsNotInitialized();

  wil::com_ptr_nothrow<ITfInputProcessorProfiles> obj;
  HRESULT result = TF_CreateInputProcessorProfiles(&obj);
  EXPECT_EQ(result, S_OK);
}

TEST(MsctfDllTest, CreateLangBarItemMgr) {
  AssertComIsNotInitialized();

  wil::com_ptr_nothrow<ITfLangBarItemMgr> obj;
  HRESULT result = TF_CreateLangBarItemMgr(&obj);
  EXPECT_EQ(result, S_OK);
}

}  // namespace
