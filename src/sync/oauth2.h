// Copyright 2010-2011, Google Inc.
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

// OAuth2Util contains utility moethods related to OAuth2.
// Please refer [1] for details about OAuth2.
//
// In OAuth2, four authorization styles are recommended.
// 1. Authorization Code Flow (4.1 in [1])
// 2. Implicit Grant Flow (4.2 in [1])
// 3. Resource Owner Password Credentials Flow (4.3 in [1])
// 4. Client Credentials Flow (4.4 in [1])
// Currently, this library supports only 1. style.
//
//
// [1] http://tools.ietf.org/html/draft-ietf-oauth-v2-15.

#ifndef MOZC_SYNC_OAUTH2_H_
#define MOZC_SYNC_OAUTH2_H_

#include <string>

#include "base/util.h"

namespace mozc {
namespace sync {

class OAuth2 {
 public:
  // Make URI to get authorize token. 'scope' and 'state' are optional in
  // protocol, but we require scope field to correspond Google API.
  // Thus, only status can be empty string.
  static void GetAuthorizeUri(const string &authorize_client_uri,
                              const string &client_id,
                              const string &redirect_uri,
                              const string &scope, const string &state,
                              string *auth_uri);

  // Get access_token and refresh_token from authorization server for OAuth2.
  // scope and state are optional in protocol, but we require scope field
  // to correspond Google API. Thus, only status can be empty string.
  // You can pass NULL to "refresh_token". The refresh token will be just
  // ignored in that case.
  // Returns true if authorization succeed, false otherwise.
  static bool AuthorizeToken(const string &authorize_token_uri,
                             const string &client_id,
                             const string &client_secret,
                             const string &redirect_uri,
                             const string &auth_token, const string &scope,
                             const string &state, string *access_token,
                             string *refresh_token);

  // Get protected resource as a string from resource server.
  // Returns true if this method gets information.
  static bool GetProtectedResource(const string &resource_uri,
                                   const string &access_token,
                                   string *output);

  // Refresh the access token and the refresh token itself.  You need
  // to specify the current refresh token to "refresh_token".  Returns
  // true if updating "access_token" and "refresh_token" succeeded.
  // In case the server doesn't update the refresh token, it does not
  // update the "refresh_token".
  static bool RefreshTokens(const string &refresh_uri,
                            const string &client_id,
                            const string &client_secret,
                            const string &scope, string* refresh_token,
                            string *access_token);


 private:
  DISALLOW_COPY_AND_ASSIGN(OAuth2);
};

}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_OAUTH2_H_
