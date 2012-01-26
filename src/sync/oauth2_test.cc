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

#include <string>

#include "base/base.h"
#include "base/util.h"
#include "net/http_client.h"
#include "net/http_client_mock.h"
#include "sync/oauth2.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace sync {

const char kClientId[] = "CLIENT_ID";
const char kClientSecret[] = "PassWord";
const char kAuthClientUri[] = "https://auth.serv/auth";
const char kAuthorizeUri[] = "https://auth.serv/token";
const char kResourceUri[] = "https://resrc.serv/full";
const char kRefreshUri[] = "https://auth.serv/refresh";
const char kRedirectUri[] = "urn:ietf:wg:oauth:2.0:oob";
const char kScope[] = "https://dom.ain/scope/";
const char kAuthToken[] = "4/correct_authorization_token";
const char kWrongAuthToken[] = "4/wrong_authorization_token_a";
const char kAccessToken[] = "1/first_correct_access_token_bbbbbbbbbbbbbbbb";
const char kRefreshToken[] = "1/first_correct_refresh_token_ccccccccccccccc";
const char kAccessToken2[] = "1/second_correct_access_token_bbbbbbbbbbbbbbb";
const char kRefreshToken2[] = "1/second_correct_refresh_token_cccccccccccccc";

class OAuth2Test : public testing::Test {
 protected:
  virtual void SetUp() {
    HTTPClient::SetHTTPClientHandler(&client_);
  }

  void SetLoginServer() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("response_type", "code"));
    params.push_back(make_pair("client_id", kClientId));
    params.push_back(make_pair("redirect_uri", kRedirectUri));
    params.push_back(make_pair("scope", kScope));
    HTTPClientMock::Result auth_result;
    auth_result.expected_url = "https://auth.serv/auth?";
    Util::AppendCGIParams(params, &auth_result.expected_url);
    auth_result.expected_result = kAuthToken;
    client_.set_result(auth_result);
  }

  void SetAuthorizationServer() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("grant_type", "authorization_code"));
    params.push_back(make_pair("client_id", kClientId));
    params.push_back(make_pair("client_secret", kClientSecret));
    params.push_back(make_pair("redirect_uri", kRedirectUri));
    params.push_back(make_pair("code", kAuthToken));
    params.push_back(make_pair("scope", kScope));

    HTTPClientMock::Result result;
    result.expected_url = kAuthorizeUri;
    Util::AppendCGIParams(params, &result.expected_request);
    result.expected_result = string("{") +
        "\"access_token\":\"" + kAccessToken + "\",\"token_type\":\"Bearer\","
        "\"expires_in\":3600,\"refresh_token\":\"" + kRefreshToken + "\"}";
    client_.set_result(result);
  }

  void SetResourceServer() {
    HTTPClientMock::Result result;
    result.expected_url = kResourceUri;
    result.expected_result = "'This is protected resource'";
    HTTPClient::Option option;
    option.include_header = true;
    option.headers.push_back(string("Authorization: OAuth ") + kAccessToken);
    client_.set_result(result);
    client_.set_option(option);
  }

  void SetRefreshServer(bool with_new_refresh_token) {
    HTTPClientMock::Result result;
    SetupRefreshTokenRequest(&result);
    result.expected_result = string("{") +
        "\"access_token\":\"" + kAccessToken2 + "\",\"token_type\":\"Bearer\","
        "\"expires_in\":3600";
    if (with_new_refresh_token) {
      result.expected_result +=
          Util::StringPrintf(",\"refresh_token\":\"%s\"", kRefreshToken2);
    }
    result.expected_result += "}";
    client_.set_result(result);
  }

  void SetRefreshServerError() {
    HTTPClientMock::Result result;
    SetupRefreshTokenRequest(&result);
    result.expected_result = "{\"error\":\"invalid_client\"}";
  }

  void SetupRefreshTokenRequest(HTTPClientMock::Result *result) {
    result->expected_url = kRefreshUri;

    vector<pair<string, string> > params;
    params.push_back(make_pair("grant_type", "refresh_token"));
    params.push_back(make_pair("client_id", kClientId));
    params.push_back(make_pair("client_secret", kClientSecret));
    params.push_back(make_pair("refresh_token", kRefreshToken));
    params.push_back(make_pair("scope", kScope));
    Util::AppendCGIParams(params, &result->expected_request);
  }

  HTTPClientMock client_;
};

TEST_F(OAuth2Test, GetAuthorizeUri) {
  SetLoginServer();

  string auth_uri;
  OAuth2::GetAuthorizeUri(kAuthClientUri, kClientId, kRedirectUri, kScope,
                          "", &auth_uri);
  EXPECT_EQ("https://auth.serv/auth?response_type=code&client_id=CLIENT%5FID&"
            "redirect_uri=urn%3Aietf%3Awg%3Aoauth%3A2%2E0%3Aoob&"
            "scope=https%3A%2F%2Fdom%2Eain%2Fscope%2F", auth_uri);
}

TEST_F(OAuth2Test, OAuth2Test) {
  SetAuthorizationServer();

  string access_token;
  string refresh_token;
  EXPECT_TRUE(OAuth2::AuthorizeToken(kAuthorizeUri, kClientId, kClientSecret,
                                     kRedirectUri, kAuthToken, kScope, "",
                                     &access_token, &refresh_token));
  EXPECT_EQ(kAccessToken, access_token);
  EXPECT_EQ(kRefreshToken, refresh_token);
}

TEST_F(OAuth2Test, InvalidAuthorizationToken) {
  SetAuthorizationServer();

  string access_token;
  string refresh_token;
  EXPECT_FALSE(OAuth2::AuthorizeToken(kAuthorizeUri, kClientId, kClientSecret,
                                      kRedirectUri, kWrongAuthToken, kScope, "",
                                      &access_token, &refresh_token));
}

TEST_F(OAuth2Test, RequestProtectedResource) {
  SetResourceServer();

  string resource;
  EXPECT_TRUE(OAuth2::GetProtectedResource(kResourceUri, kAccessToken,
                                           &resource));
  EXPECT_EQ("'This is protected resource'", resource);
  EXPECT_FALSE(OAuth2::GetProtectedResource(kResourceUri, kAccessToken2,
                                            &resource));
}

TEST_F(OAuth2Test, RefreshToken) {
  SetRefreshServer(true);

  string access_token(kAccessToken);
  string refresh_token(kRefreshToken);
  EXPECT_TRUE(OAuth2::RefreshTokens(kRefreshUri, kClientId, kClientSecret,
                                    kScope, &refresh_token, &access_token));
  EXPECT_EQ(kAccessToken2, access_token);
  EXPECT_EQ(kRefreshToken2, refresh_token);
}

TEST_F(OAuth2Test, RefreshTokenWithoutNewRefreshToken) {
  SetRefreshServer(false);

  string access_token(kAccessToken);
  string refresh_token(kRefreshToken);
  EXPECT_TRUE(OAuth2::RefreshTokens(kRefreshUri, kClientId, kClientSecret,
                                    kScope, &refresh_token, &access_token));
  EXPECT_EQ(kAccessToken2, access_token);
  // Refresh token is not updated.
  EXPECT_EQ(kRefreshToken, refresh_token);
}

TEST_F(OAuth2Test, RefreshTokenError) {
  SetRefreshServerError();

  string access_token(kAccessToken);
  string refresh_token(kRefreshToken);
  EXPECT_FALSE(OAuth2::RefreshTokens(kRefreshUri, kClientId, kClientSecret,
                                     kScope, &refresh_token, &access_token));
  // Tokens unchanged.
  EXPECT_EQ(kAccessToken, access_token);
  EXPECT_EQ(kRefreshToken, refresh_token);
}

}  // namespace sync
}  // namespace mozc
