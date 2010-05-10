// Copyright 2010, Google Inc.
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

#include "base/base.h"
#include "base/password_manager.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);
DECLARE_bool(use_mock_password_manager);

namespace mozc {
TEST(PasswordManager, PasswordManagerTest) {
// NOTE(komatsu, mukai): Since DefaultPasswordManager on Mac requires
// keychain access with a dialog window, we use mock password manager
// instead only for Mac.
#ifdef OS_MACOSX
  kUseMockPasswordManager = true;
#else
  kUseMockPasswordManager = false;
#endif
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  string password1, password2;
  EXPECT_TRUE(PasswordManager::InitPassword());
  EXPECT_TRUE(PasswordManager::GetPassword(&password1));
  EXPECT_TRUE(PasswordManager::GetPassword(&password2));
  EXPECT_EQ(password1, password2);
  EXPECT_TRUE(PasswordManager::RemovePassword());
  EXPECT_TRUE(PasswordManager::InitPassword());
  EXPECT_TRUE(PasswordManager::GetPassword(&password2));
  EXPECT_NE(password1, password2);

  EXPECT_TRUE(PasswordManager::RemovePassword());
  EXPECT_TRUE(PasswordManager::GetPassword(&password1));
  EXPECT_TRUE(PasswordManager::GetPassword(&password2));
  EXPECT_EQ(password1, password2);
}
}  // namespace mozc
