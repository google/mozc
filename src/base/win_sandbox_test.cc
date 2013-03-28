// Copyright 2010-2013, Google Inc.
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

#include "base/win_sandbox.h"
#include "base/scoped_handle.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {
void VerifySidContained(const vector<Sid> sids,
                        WELL_KNOWN_SID_TYPE expected_well_known_sid) {
  Sid expected_sid(expected_well_known_sid);
  for (size_t i = 0; i < sids.size(); ++i) {
    Sid temp_sid = sids[i];
    if (::EqualSid(expected_sid.GetPSID(), temp_sid.GetPSID())) {
      // Found!
      return;
    }
  }
  EXPECT_TRUE(false) << "Not found. Expected SID: " << expected_well_known_sid;
}
}  // anonymous namespace

TEST(WinSandboxTest, GetSidsToDisable) {
  HANDLE process_token_ret = NULL;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const vector<Sid> lockdown = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const vector<Sid> restricted = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const vector<Sid> limited = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_LIMITED);
  const vector<Sid> interactive = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const vector<Sid> non_admin = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const vector<Sid> restricted_same_access = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const vector<Sid> unprotect = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_TRUE(restricted.size() == lockdown.size());
  VerifySidContained(lockdown, WinBuiltinUsersSid);

  VerifySidContained(limited, WinAuthenticatedUserSid);

  EXPECT_TRUE(non_admin.size() == interactive.size());

  EXPECT_EQ(0, restricted_same_access.size());

  EXPECT_EQ(0, unprotect.size());
}

TEST(WinSandboxTest, GetPrivilegesToDisable) {
  HANDLE process_token_ret = NULL;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const vector<LUID> lockdown = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const vector<LUID> restricted = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const vector<LUID> limited = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_LIMITED);
  const vector<LUID> interactive = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const vector<LUID> non_admin = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const vector<LUID> restricted_same_access =
      WinSandbox::GetPrivilegesToDisable(
          process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const vector<LUID> unprotect = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_EQ(0, restricted_same_access.size());
  EXPECT_EQ(0, unprotect.size());
}

TEST(WinSandboxTest, GetSidsToRestrict) {
  HANDLE process_token_ret = NULL;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const vector<Sid> lockdown = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const vector<Sid> restricted = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const vector<Sid> limited = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_LIMITED);
  const vector<Sid> interactive = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const vector<Sid> non_admin = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const vector<Sid> restricted_same_access = WinSandbox::GetSidsToRestrict(
          process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const vector<Sid> unprotect = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_EQ(1, lockdown.size());
  VerifySidContained(lockdown, WinNullSid);

  VerifySidContained(limited, WinBuiltinUsersSid);

  VerifySidContained(interactive, WinBuiltinUsersSid);
}
}  // namespace mozc
