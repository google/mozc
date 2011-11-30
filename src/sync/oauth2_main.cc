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

#include <iomanip>
#include <iostream>

#include "base/base.h"
#include "base/util.h"
#include "sync/oauth2.h"
#include "third_party/jsoncpp/json.h"

// Program dependent constants
// You have to get Client Credentials for Goole APIs and replace "client_id"
// and "client_secret" below with them.
DEFINE_string(client_id, "client_id", "Client ID for OAuth2");
DEFINE_string(client_secret, "client_secret", "Client password for OAuth2");
DEFINE_string(authorize_client_uri,
              "https://accounts.google.com/o/oauth2/auth",
              "URI to authorize client program");
DEFINE_string(redirect_uri, "urn:ietf:wg:oauth:2.0:oob",
              "Redirect URI when authorization passed");
DEFINE_string(authorize_token_uri,
              "https://accounts.google.com/o/oauth2/token",
              "URI to get access token and refresh token");
DEFINE_string(resource_uri,
              "https://www.google.com/m8/feeds/contacts/default/full",
              "URI to get protected resource");
DEFINE_string(scope, "https://www.google.com/m8/feeds/", "Scope of OAuth2");
DEFINE_string(state, "", "Maintain state between request and callback");

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  string auth_uri;
  mozc::sync::OAuth2::GetAuthorizeUri(FLAGS_authorize_client_uri,
                                      FLAGS_client_id,
                                      FLAGS_redirect_uri,
                                      FLAGS_scope, FLAGS_state, &auth_uri);
  LOG(INFO) << "Access this URI with your browser and approve it." << endl;
  LOG(INFO) << auth_uri << endl;

  // Authorization phase
  string auth_token;
  LOG(INFO) << "Input authorize token : ";
  cin >> auth_token;

  string access_token;
  string refresh_token;
  CHECK(mozc::sync::OAuth2::AuthorizeToken(FLAGS_authorize_token_uri,
                                           FLAGS_client_id, FLAGS_client_secret,
                                           FLAGS_redirect_uri, auth_token,
                                           FLAGS_scope, FLAGS_state,
                                           &access_token, &refresh_token));

  LOG(INFO) << "Access token : " << access_token << endl;
  LOG(INFO) << "Refresh token : " << refresh_token << endl;

  // Application example : Make name and mail address list from contact list.
  // Get information from Google server.
  string resource_uri = FLAGS_resource_uri + "?";
  vector<pair<string, string> > params;
  params.push_back(make_pair("alt", "json"));
  params.push_back(make_pair("v", "3.0"));
  mozc::Util::AppendCGIParams(params, &resource_uri);

  string json;
  CHECK(mozc::sync::OAuth2::GetProtectedResource(resource_uri,
                                                 access_token, &json));

  // Parse JSON script.
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse(json, root)) {
    LOG(ERROR) << "Parsing contact information failed.";
    return 0;
  }

  Json::Value members(root["feed"]["entry"]);
  for (Json::Value::iterator it = members.begin();
       it != members.end(); ++it) {
    Json::Value &name = (*it)["gd$name"];
    string kanji;
    string yomi;
    if (name.isMember("gd$familyName")) {
      Json::Value &family = name["gd$familyName"];
      if (family.isMember("$t")) {
        kanji += family["$t"].asString();
      }
      if (family.isMember("yomi")) {
        yomi += family["yomi"].asString();
      }
    }
    if (name.isMember("gd$givenName")) {
      Json::Value &given = name["gd$givenName"];
      if (given.isMember("$t")) {
        kanji += given["$t"].asString();
      }
      if (given.isMember("yomi")) {
        yomi += given["yomi"].asString();
      }
    }

    // Output information about a friend.
    if (kanji.empty() && yomi.empty()) {
      continue;
    }
    LOG(INFO) << kanji;
    if (!yomi.empty()) {
      LOG(INFO) << "(" << yomi << ")";
    }
    LOG(INFO) << " <" << (*it)["gd$email"][0]["address"].asString()
              << ">" << endl;
  }

  return 0;
}
