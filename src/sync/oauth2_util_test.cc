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

#include "base/util.h"
#include "net/http_client_mock.h"
#include "storage/registry.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {
namespace {
const char kAuthorizeClientUrl[] = "https://accounts.google.com/o/oauth2/auth";
const char kRedirectUri[] = "urn:ietf:wg:oauth:2.0:oob";
const char kAuthorizeTokenUri[] = "https://accounts.google.com/o/oauth2/token";
const char kScope[] = "https://www.googleapis.com/auth/imesync";

const char kAuthToken[] = "4/correct_authorization_token";
const char kWrongAuthToken[] = "4/wrong_authorization_token_a";
const char kAccessToken[] = "1/first_correct_access_token_bbbbbbbbbbbbbbbb";
const char kRefreshToken[] = "1/first_correct_refresh_token_ccccccccccccccc";
const char kAccessToken2[] = "1/second_correct_access_token_bbbbbbbbbbbbbbb";
const char kRefreshToken2[] = "1/second_correct_refresh_token_cccccccccccccc";
const char kResourceUri[] =
    "https://www.google.com/m8/feeds/contacts/default/full";
}  // namespace

class OAuth2UtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    HTTPClient::SetHTTPClientHandler(&client_);
    oauth2_client_.reset(new OAuth2Client(
        "test", "dummyclientid", "dummyclientsecret"));
  }

  void SetAuthorizationServer() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("grant_type", "authorization_code"));
    params.push_back(make_pair("client_id", oauth2_client_->client_id_));
    params.push_back(make_pair(
        "client_secret", oauth2_client_->client_secret_));
    params.push_back(make_pair("redirect_uri", kRedirectUri));
    params.push_back(make_pair("code", kAuthToken));
    params.push_back(make_pair("scope", kScope));

    HTTPClientMock::Result result;
    result.expected_url = kAuthorizeTokenUri;
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

  void SetRefreshServer() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("grant_type", "refresh_token"));
    params.push_back(make_pair("client_id", oauth2_client_->client_id_));
    params.push_back(make_pair(
        "client_secret", oauth2_client_->client_secret_));
    params.push_back(make_pair("refresh_token", kRefreshToken));
    params.push_back(make_pair("scope", kScope));
    HTTPClientMock::Result result;
    result.expected_url = kAuthorizeTokenUri;
    Util::AppendCGIParams(params, &result.expected_request);
    result.expected_result = string("{") +
        "\"access_token\":\"" + kAccessToken2 + "\",\"token_type\":\"Bearer\","
        "\"expires_in\":3600,\"refresh_token\":\"" + kRefreshToken2 + "\"}";
    client_.set_result(result);
  }

  HTTPClientMock client_;
  scoped_ptr<OAuth2Client> oauth2_client_;
};

TEST_F(OAuth2UtilTest, GetLoginUri) {
  OAuth2Util oauth2(oauth2_client_.get());

  string uri = oauth2.GetAuthenticateUri();
  EXPECT_EQ(Util::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth?response_type=code&"
      "client_id=%s&redirect_uri=urn%%3Aietf%%3Awg%%3Aoauth%%3A2%%2E0%%3Aoob&"
      "scope=https%%3A%%2F%%2Fwww%%2Egoogleapis%%2Ecom%%2Fauth%%2Fimesync",
      oauth2_client_->client_id_.c_str()), uri);
}

TEST_F(OAuth2UtilTest, CheckLogin) {
  OAuth2Util oauth2(oauth2_client_.get());
  SetAuthorizationServer();
  EXPECT_TRUE(oauth2.RequestAccessToken(kAuthToken));

  string access_token;
  string refresh_token;
  EXPECT_TRUE(oauth2.GetTokens(&access_token, &refresh_token));
  EXPECT_EQ(kAccessToken, access_token);
  EXPECT_EQ(kRefreshToken, refresh_token);
}

TEST_F(OAuth2UtilTest, GetResource) {
  OAuth2Util oauth2(oauth2_client_.get());
  SetResourceServer();
  string resource;
  EXPECT_TRUE(oauth2.RegisterTokens(kAccessToken, kRefreshToken));
  EXPECT_TRUE(oauth2.RequestResource(kResourceUri, &resource));
  EXPECT_EQ("'This is protected resource'", resource);
}

TEST_F(OAuth2UtilTest, RefeshToken) {
  OAuth2Util oauth2(oauth2_client_.get());
  string access_token = kAccessToken;
  string refresh_token = kRefreshToken;
  SetRefreshServer();
  EXPECT_TRUE(oauth2.RegisterTokens(access_token, refresh_token));
  EXPECT_TRUE(oauth2.RefreshAccessToken());
  EXPECT_TRUE(oauth2.GetTokens(&access_token, &refresh_token));
  EXPECT_EQ(kAccessToken2, access_token);
  EXPECT_EQ(kRefreshToken2, refresh_token);
}

}  // namespace sync
}  // namespace mozc
