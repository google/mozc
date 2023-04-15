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

#include "base/win32/com.h"

#include <objbase.h>
#include <rpc.h>
#include <shobjidl.h>
#include <unknwn.h>
#include <wrl.h>

#include <utility>

#include "base/win32/scoped_com.h"
#include "testing/gunit.h"

namespace mozc::win32 {
namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

// Mock interfaces for testing.
MIDL_INTERFACE("A03A80F4-9254-4C8B-AF25-0674FCED18E5")
IMock1 : public IUnknown { STDMETHOD(Test1)() = 0; };

MIDL_INTERFACE("863EF391-8485-4257-8423-8D919D1AE8DC")
IMock2 : public IUnknown { STDMETHOD(Test2)() = 0; };

MIDL_INTERFACE("7CC0C082-8CA5-4A87-97C4-4FC14FBCE0B3")
IDerived : public IMock1 { STDMETHOD(Derived()) = 0; };

class Mock
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IMock2, IDerived> {
 public:
  Mock() { ++instance_count_; }
  ~Mock() override { --instance_count_; }

  STDMETHODIMP QueryInterface(REFIID iid, void** out) override {
    qi_count_++;
    return RuntimeClass::QueryInterface(iid, out);
  }
  STDMETHODIMP Test1() override { return S_OK; }
  STDMETHODIMP Test2() override { return S_FALSE; }
  STDMETHODIMP Derived() override { return 2; }

  static void Reset() {
    instance_count_ = 0;
    qi_count_ = 0;
  }
  static int GetInstanceCount() { return instance_count_; }
  static int GetQICountAndReset() { return std::exchange(qi_count_, 0); }

 private:
  static int instance_count_;
  static int qi_count_;
};

int Mock::instance_count_;
int Mock::qi_count_;

class ComTest : public ::testing::Test {
 protected:
  ComTest() { Mock::Reset(); }
  ~ComTest() override { EXPECT_EQ(Mock::GetInstanceCount(), 0); }

 private:
  ScopedCOMInitializer initializer_;
};

TEST_F(ComTest, ComCreateInstance) {
  ComPtr<IShellLink> shellink = ComCreateInstance<IShellLink, ShellLink>();
  EXPECT_TRUE(shellink);
  EXPECT_TRUE(ComCreateInstance<IShellLink>(CLSID_ShellLink));
  EXPECT_FALSE(ComCreateInstance<IShellFolder>(CLSID_ShellLink));
}

TEST_F(ComTest, ComQuery) {
  ComPtr<IMock1> mock1(Microsoft::WRL::Make<Mock>());
  EXPECT_TRUE(mock1);
  EXPECT_EQ(mock1->Test1(), S_OK);

  ComPtr<IDerived> derived = ComQuery<IDerived>(mock1);
  EXPECT_TRUE(derived);
  EXPECT_EQ(derived->Derived(), 2);
  EXPECT_EQ(Mock::GetQICountAndReset(), 1);

  EXPECT_TRUE(ComQuery<IMock1>(derived));
  EXPECT_EQ(Mock::GetQICountAndReset(), 0);

  ComPtr<IMock2> mock2 = ComQuery<IMock2>(mock1);
  EXPECT_TRUE(mock2);
  EXPECT_EQ(mock2->Test2(), S_FALSE);
  EXPECT_EQ(Mock::GetQICountAndReset(), 1);

  mock2 = ComQuery<IMock2>(mock1.Get());
  EXPECT_TRUE(mock2);
  EXPECT_EQ(mock2->Test2(), S_FALSE);
  EXPECT_EQ(Mock::GetQICountAndReset(), 1);

  EXPECT_FALSE(ComQuery<IShellView>(mock2));
  EXPECT_EQ(Mock::GetQICountAndReset(), 1);
}

TEST_F(ComTest, ComCopy) {
  ComPtr<IMock1> mock1(Microsoft::WRL::Make<Mock>());
  EXPECT_TRUE(mock1);
  EXPECT_EQ(mock1->Test1(), S_OK);

  ComPtr<IUnknown> unknown = ComCopy<IUnknown>(mock1);
  EXPECT_TRUE(unknown);
  EXPECT_EQ(Mock::GetQICountAndReset(), 0);

  EXPECT_FALSE(ComCopy<IShellLink>(unknown));
  EXPECT_EQ(Mock::GetQICountAndReset(), 1);

  IUnknown* null = nullptr;
  EXPECT_FALSE(ComCopy<IUnknown>(null));
}

}  // namespace
}  // namespace mozc::win32
