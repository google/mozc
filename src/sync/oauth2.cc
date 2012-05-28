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

#include "sync/oauth2.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "net/http_client.h"
#include "net/jsoncpp.h"
#include "storage/registry.h"

namespace mozc {
namespace sync {
namespace {
// Constants defined in OAuth2 protocol
const char kOAuth2ContentType[] = "application/x-www-form-urlencoded";
const char kOAuth2AccessTokenKey[] = "access_token";
const char kOAuth2RefreshTokenKey[] = "refresh_token";
}  // namespace

void OAuth2::GetAuthorizeUri(const string &authorize_client_uri,
                             const string &client_id,
                             const string &redirect_uri,
                             const string &scope, const string &state,
                             string *auth_uri) {
  DCHECK(auth_uri);

  vector<pair<string, string> > params;
  params.push_back(make_pair("response_type", "code"));
  params.push_back(make_pair("client_id", client_id));
  params.push_back(make_pair("redirect_uri", redirect_uri));
  if (!scope.empty()) {
    params.push_back(make_pair("scope", scope));
  }
  if (!state.empty()) {
    params.push_back(make_pair("state", state));
  }

  *auth_uri = authorize_client_uri + "?";
  Util::AppendCGIParams(params, auth_uri);
}

bool OAuth2::AuthorizeToken(const string &authorize_token_uri,
                            const string &client_id,
                            const string &client_secret,
                            const string &redirect_uri,
                            const string &auth_token, const string &scope,
                            const string &state, string *access_token,
                            string *refresh_token) {
  DCHECK(access_token);

  vector<pair<string, string> > params;
  // REQUIRED parameters
  params.push_back(make_pair("grant_type", "authorization_code"));
  params.push_back(make_pair("client_id", client_id));
  params.push_back(make_pair("client_secret", client_secret));
  params.push_back(make_pair("redirect_uri", redirect_uri));
  params.push_back(make_pair("code", auth_token));
  // OPTIONAL parameters
  if (!scope.empty()) {
    params.push_back(make_pair("scope", scope));
  }
  if (!state.empty()) {
    params.push_back(make_pair("state", state));
  }

  string request;
  Util::AppendCGIParams(params, &request);
  VLOG(2) << "Request to server:" << request;
  mozc::HTTPClient::Option option;
  option.headers.push_back(string("Content-Type: ") + kOAuth2ContentType);
  string response;
  if (!mozc::HTTPClient::Post(authorize_token_uri,
                              request, option, &response)) {
    LOG(ERROR) << "Cannot connect to " << authorize_token_uri
               << " or bad request.";
    return false;
  }

  // Parse JSON and save access_token and refresh_token
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(response, root)) {
    LOG(INFO) << "Parsing JSON error.";
    return false;
  }
  if (!root.isMember(kOAuth2AccessTokenKey)) {
    LOG(ERROR) << "Cannot find access_token "
               << "in response from authorized server.";
    return false;
  }

  *access_token = root[kOAuth2AccessTokenKey].asString();
  if (NULL != refresh_token && root.isMember(kOAuth2RefreshTokenKey)) {
    *refresh_token = root[kOAuth2RefreshTokenKey].asString();
  }

  return true;
}

// Currently, this method corresponds with only one style of requesting
// while OAuth2 defines several access token types.
// TODO(peria): Make generalized accessor(s)
bool OAuth2::GetProtectedResource(const string &resource_uri,
                                  const string &access_token,
                                  string *output) {
  DCHECK(output);

  mozc::HTTPClient::Option option;
  option.headers.push_back("Authorization: OAuth " + access_token);
  return mozc::HTTPClient::Get(resource_uri, option, output);
}

bool OAuth2::RefreshTokens(const string &refresh_uri,
                           const string &client_id,
                           const string &client_secret,
                           const string &scope,
                           string *refresh_token,
                           string *access_token) {
  DCHECK(refresh_token);
  DCHECK(access_token);

  vector<pair<string, string> > params;
  // REQUIRED parameters
  params.push_back(make_pair("grant_type", "refresh_token"));
  params.push_back(make_pair("client_id", client_id));
  params.push_back(make_pair("client_secret", client_secret));
  params.push_back(make_pair("refresh_token", *refresh_token));
  if (!scope.empty()) {
    params.push_back(make_pair("scope", scope));
  }

  string request;
  Util::AppendCGIParams(params, &request);
  VLOG(2) << "Request to server:" << request;
  mozc::HTTPClient::Option option;
  option.headers.push_back(string("Content-Type: ") + kOAuth2ContentType);
  string response;
  if (!mozc::HTTPClient::Post(refresh_uri, request, option, &response)) {
    LOG(ERROR) << "Cannot connect to " << refresh_uri << " or bad request.";
    return false;
  }

  LOG(INFO) << response;
  // Parse JSON and save access_token and refresh_token
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(response, root)) {
    LOG(INFO) << "Parsing JSON error.";
    return false;
  }
  if (!root.isMember(kOAuth2AccessTokenKey)) {
    LOG(ERROR) << "cannot find " << kOAuth2AccessTokenKey
               << " in response from authorized server.";
    return false;
  }
  *access_token = root[kOAuth2AccessTokenKey].asString();

  if (root.isMember(kOAuth2RefreshTokenKey)) {
    *refresh_token = root[kOAuth2RefreshTokenKey].asString();
  } else {
    LOG(INFO) << "cannot find " << kOAuth2RefreshTokenKey
              << " in response from authorized server.";
    // This is not an error so pass through.
  }

  return true;
}

};  // namespace sync
};  // namespace mozc
