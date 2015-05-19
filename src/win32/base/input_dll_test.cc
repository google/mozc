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

#include <string>

#include "base/system_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/input_dll.h"

namespace mozc {
namespace win32 {

class InputDllTest : public testing::Test {
 public:
 protected:
  InputDllTest() {}

  virtual ~InputDllTest() {}

  virtual void SetUp() {
    // TODO(yukawa): Implement injection mechanism to ::LoadLibrary API
    // TODO(yukawa): Inject custom LoadLibrary to make this test independent
    //   of test environment.
  }

  virtual void TearDown() {
    // TODO(yukawa): Implement injection mechanism to ::LoadLibrary API
    // TODO(yukawa): Remove custom LoadLibrary injection not to affect
    //   subsequent tests.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputDllTest);
};

// Currently this test is not independent of test environment.
// TODO(yukawa): Implement injection mechanism to ::LoadLibrary API to make
//   the code flow predictable.
TEST_F(InputDllTest, EnsureInitializedTest) {
  if (InputDll::EnsureInitialized()) {
    // Check internal status.
    EXPECT_FALSE(InputDll::not_found_);
    // gtest will cause compilation error if we use <volatile HMODULE> here.
    // Use <void *> instead.
    EXPECT_NE(nullptr, static_cast<void *>(InputDll::module_));

    // Actually input.dll exists on Windows XP.  However, it does not always
    // mean that input.dll exports the functions in which we are interested.

    if (SystemUtil::IsVistaOrLater()) {
      // Assume that the following funcsions are available on Vista and later.
      EXPECT_NE(nullptr, InputDll::enum_enabled_layout_or_tip());
      EXPECT_NE(nullptr, InputDll::enum_layout_or_tip_for_setup());
      EXPECT_NE(nullptr, InputDll::install_layout_or_tip());
      EXPECT_NE(nullptr, InputDll::install_layout_or_tip_user_reg());
      EXPECT_NE(nullptr, InputDll::set_default_layout_or_tip());
    } else {
      // Assume that the following funcsions are not available on XP and prior.
      EXPECT_EQ(nullptr, InputDll::enum_enabled_layout_or_tip());
      EXPECT_EQ(nullptr, InputDll::enum_layout_or_tip_for_setup());
      EXPECT_EQ(nullptr, InputDll::install_layout_or_tip_user_reg());
      EXPECT_EQ(nullptr, InputDll::set_default_layout_or_tip());
    }

    // Check the consistency of the retuls of second call.
    EXPECT_TRUE(InputDll::EnsureInitialized());
    return;
  }

  // Check internal status.
  EXPECT_TRUE(InputDll::not_found_);
  // gtest will cause compilation error if we use <volatile HMODULE> here.
  // Use <void *> instead.
  EXPECT_EQ(nullptr, static_cast<void *>(InputDll::module_));

  EXPECT_EQ(nullptr, InputDll::enum_enabled_layout_or_tip());
  EXPECT_EQ(nullptr, InputDll::enum_layout_or_tip_for_setup());
  EXPECT_EQ(nullptr, InputDll::install_layout_or_tip());
  EXPECT_EQ(nullptr, InputDll::install_layout_or_tip_user_reg());
  EXPECT_EQ(nullptr, InputDll::set_default_layout_or_tip());

  // Check the consistency of the retuls of second call.
  EXPECT_FALSE(InputDll::EnsureInitialized());
}

}  // namespace win32
}  // namespace mozc
