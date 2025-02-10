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

#include "base/win32/com_implements.h"

#include <guiddef.h>
#include <objbase.h>
#include <shobjidl.h>
#include <unknwn.h>
#include <windows.h>

#include "base/win32/com.h"
#include "testing/gunit.h"
#include "third_party/wil/include/wil/com.h"

namespace mozc::win32 {
namespace {

// Mock interfaces for testing.
MIDL_INTERFACE("A03A80F4-9254-4C8B-AF25-0674FCED18E5")
IMock1 : public IUnknown {
  virtual ~IMock1() = default;
  STDMETHOD(Test1)() = 0;
};

MIDL_INTERFACE("863EF391-8485-4257-8423-8D919D1AE8DC")
IMock2 : public IUnknown {
  virtual ~IMock2() = default;
  STDMETHOD(Test2)() = 0;
};

MIDL_INTERFACE("7CC0C082-8CA5-4A87-97C4-4FC14FBCE0B3")
IDerived : public IMock1 {
  virtual ~IDerived() = default;
  STDMETHOD(Derived()) = 0;
};

MIDL_INTERFACE("F2B8DCC5-226C-4123-8F78-2BC36B574629")
IDerivedDerived : public IDerived { virtual ~IDerivedDerived() = default; };

MIDL_INTERFACE("9C1A7121-BF54-4826-856E-55A90864EE64")
IRefCount : public IUnknown {
  virtual ~IRefCount() = default;
  STDMETHOD_(ULONG, RefCount)() = 0;
};

}  // namespace

// This specialization needs to be in the mozc::win32 namespace.
template <>
bool IsIIDOf<IDerivedDerived>(REFIID riid) {
  return IsIIDOf<IDerivedDerived, IDerived, IMock1>(riid);
}

namespace {
struct MockTraits : public ComImplementsTraits {
  static void OnObjectRelease(ULONG ref) {
    called = true;
    MockTraits::ref = ref;
  }

  static bool called;
  static ULONG ref;
};

bool MockTraits::called = false;
ULONG MockTraits::ref = 0;

class Mock
    : public ComImplements<MockTraits, IMock2, IDerivedDerived, IRefCount> {
 public:
  STDMETHODIMP Test1() override { return 1; }
  STDMETHODIMP Test2() override { return 2; }
  STDMETHODIMP Derived() override { return 3; }
  STDMETHODIMP_(ULONG) RefCount() override {
    AddRef();
    return Release();
  }
};

class ComImplementsTest : public testing::Test {
 public:
  ComImplementsTest() { MockTraits::called = false; }

  ~ComImplementsTest() override {
    EXPECT_EQ(CanComModuleUnloadNow(), S_OK);
    EXPECT_EQ(MockTraits::ref, 0);
    EXPECT_TRUE(MockTraits::called);
  }
};

TEST_F(ComImplementsTest, ReferenceCount) {
  auto mock = MakeComPtr<Mock>();
  EXPECT_EQ(CanComModuleUnloadNow(), S_FALSE);
  EXPECT_EQ(mock->RefCount(), 1);
  EXPECT_EQ(mock->AddRef(), 2);
  EXPECT_EQ(mock->Release(), 1);
}

TEST_F(ComImplementsTest, QueryInterface) {
  auto mock = MakeComPtr<Mock>();
  EXPECT_EQ(CanComModuleUnloadNow(), S_FALSE);
  EXPECT_EQ(mock->Test1(), 1);
  EXPECT_EQ(mock->Test2(), 2);
  EXPECT_EQ(mock->Derived(), 3);
  wil::com_ptr_nothrow<IMock1> mock1;
  EXPECT_HRESULT_SUCCEEDED(mock->QueryInterface(IID_PPV_ARGS(mock1.put())));
  EXPECT_TRUE(mock1);
  wil::com_ptr_nothrow<IMock2> mock2;
  EXPECT_HRESULT_SUCCEEDED(mock1->QueryInterface(IID_PPV_ARGS(mock2.put())));
  EXPECT_TRUE(mock2);
  wil::com_ptr_nothrow<IUnknown> unknown;
  EXPECT_HRESULT_SUCCEEDED(mock->QueryInterface(IID_PPV_ARGS(unknown.put())));
  EXPECT_TRUE(unknown);
  wil::com_ptr_nothrow<IDerivedDerived> derived_derived;
  EXPECT_HRESULT_SUCCEEDED(
      unknown->QueryInterface(IID_PPV_ARGS(derived_derived.put())));
  EXPECT_TRUE(derived_derived);

  void *p = mock.get();
  EXPECT_EQ(mock->QueryInterface(IID_IShellItem, &p), E_NOINTERFACE);
  EXPECT_EQ(mock->QueryInterface(IID_IUnknown, nullptr), E_POINTER);
  wil::com_ptr_nothrow<IDerived> derived;
  EXPECT_HRESULT_SUCCEEDED(
      unknown->QueryInterface(IID_PPV_ARGS(derived.put())));
  EXPECT_TRUE(derived);
  EXPECT_EQ(derived->Derived(), 3);
  EXPECT_EQ(CanComModuleUnloadNow(), S_FALSE);
}

class SingleMock : public ComImplements<MockTraits, IMock1> {
 public:
  STDMETHODIMP Test1() override { return 1; }
};

TEST_F(ComImplementsTest, SingleMock) {
  auto mock = MakeComPtr<SingleMock>();
  EXPECT_EQ(CanComModuleUnloadNow(), S_FALSE);
  wil::com_ptr_nothrow<IMock1> mock1;
  EXPECT_HRESULT_SUCCEEDED(mock->QueryInterface(IID_PPV_ARGS(mock1.put())));
  EXPECT_TRUE(mock1);
  wil::com_ptr_nothrow<IUnknown> unknown;
  EXPECT_HRESULT_SUCCEEDED(mock1->QueryInterface(IID_PPV_ARGS(unknown.put())));
  EXPECT_TRUE(unknown);
  wil::com_ptr_nothrow<IDerived> derived;
  EXPECT_EQ(unknown->QueryInterface(IID_PPV_ARGS(derived.put())),
            E_NOINTERFACE);
  EXPECT_FALSE(derived);
  EXPECT_EQ(CanComModuleUnloadNow(), S_FALSE);
}

}  // namespace
}  // namespace mozc::win32
