// Copyright 2010-2012, Google Inc.
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

#include "sync/oauth2_token_util.h"

#include "testing/base/public/gunit.h"

namespace mozc {
namespace sync {
TEST(OAuth2TokenUtil, ParseAuthCodeFromWindowTitleForWindows) {
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
          "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - Google Chrome"));
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
          "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - Mozilla Firefox"));
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
          "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - Opera"));
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
           "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - "
           "Windows Internet Explorer"));

  EXPECT_EQ("",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
           "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5"));
  EXPECT_EQ("",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForWindows(
           "4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - Opera"));
}

TEST(OAuth2TokenUtil, ParseAuthCodeFromWindowTitleForMac) {
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForMac(
          "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5"));
  EXPECT_EQ("4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - foo",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForMac(
          "Success code=4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5 - foo"));
  EXPECT_EQ("",
      OAuth2TokenUtil::ParseAuthCodeFromWindowTitleForMac(
           "4/a1B2c3D4e5F6g7H8i9J1k2l3M4n5"));
}
}  // sync
}  // mozc
