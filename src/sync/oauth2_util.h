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

#ifndef MOZC_SYNC_OAUTH2_UTIL_H_
#define MOZC_SYNC_OAUTH2_UTIL_H_

#include "sync/oauth2.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {
struct OAuth2Client;

// OAuth2Util class is a wrapper class of OAuth2. It simplifies authorization
// interface to use specific servers with an OAuth2 authorization.
// Necessary tokens in accessing servers are stored in local storage.
// TODO(peria): generalize for other than Google servers.
class OAuth2Util {
 public:
  OAuth2Util(const OAuth2Client *client);
  ~OAuth2Util();

  // Return a URI to authorize mozc via web browser.
  string GetAuthenticateUri();

  // Requests an access_token with the authorization_token and stores the access
  // token into the mozc registry.  Returns true if it successfully obtains the
  // access token.  Returns false otherwise.
  bool RequestAccessToken(const string &auth_token, OAuth2::Error *error);

  // Refreshes an access token in the local storage, and stores the new token.
  // Return true only if refresh succeeds.
  bool RefreshAccessToken(OAuth2::Error *error);

  // Accesses 'resource_uri' and puts returned string in 'resource'. This
  // method does not refresh tokens even if it fails, so you need to refresh it
  // by yourself. Returns true if getting resouce succeeds, or false otherwise.
  // TODO(peria): enable to use POST method
  bool RequestResource(const string &resource_uri, string *resource);

  // Clear all registed tokens.
  void Clear();

  // Get the access token from the local storage and stores it to
  // "access_token".  Returns true if it successfully obtain the
  // access token.
  bool GetAccessToken(string *access_token);

  // Get the machine ID and stores it to "mid".  If the underlaying
  // storage does not have the machine id, it generates the id
  // randomly and stores the generated id.  Returns false only if the
  // new machine id generation fails.
  bool GetMID(string *mid);

  // Change the scope of authentification.
  // This method is used only in tests.
  void set_scope(const string &scope);

  // Following getter functions are used in only unit tests.
  string authenticate_uri_for_unittest() {
    return authenticate_uri_;
  }
  string redirect_uri_for_unittest() {
    return redirect_uri_;
  }
  string request_token_uri_for_unittest() {
    return request_token_uri_;
  }
  string scope_for_unittest() {
    return scope_;
  }

 private:
  const OAuth2Client *client_;
  const string authenticate_uri_;
  const string redirect_uri_;
  const string request_token_uri_;
  string scope_;

  FRIEND_TEST(OAuth2UtilTest, CheckLogin);
  FRIEND_TEST(OAuth2UtilTest, GetResource);
  FRIEND_TEST(OAuth2UtilTest, RefeshToken);

  DISALLOW_COPY_AND_ASSIGN(OAuth2Util);

  // Gets the access token and the refresh token from the local storage.
  // Returns true if both tokens are found, or false otherwise.
  bool GetTokens(string *access_token, string *refresh_token);

  // Registers the access token and the refresh token into the local storage.
  // Returns true if all steps succeed, or false otherwise.
  bool RegisterTokens(const string &access_token, const string &refresh_token);

  // Encrypts or decrypts a string.
  bool EncryptString(const string &plain, string *crypt);
  bool DecryptString(const string &crypt, string *plain);

  // Gets the key in the local storage for the access token or the refresh
  // token.
  string GetAccessKey();
  string GetRefreshKey();

  // Initializes a new mahcine id and stores it to the underlaying
  // storage.  Returns true if it successfully generated the id.
  bool InitMID();
};
}  // namespace sync
}  // namespace mozc
#endif  // MOZC_SYNC_OAUTH2_UTIL_H_
