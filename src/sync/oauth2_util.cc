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

#include "sync/oauth2_util.h"

#include <string>

#include "base/encryptor.h"
#include "base/password_manager.h"
#include "storage/registry.h"
#include "sync/oauth2.h"
#include "sync/oauth2_client.h"

namespace mozc {
namespace sync {
namespace {
const char kAuthenticateUri[] = "https://accounts.google.com/o/oauth2/auth";
const char kRedirectUri[] = "urn:ietf:wg:oauth:2.0:oob";
const char kRequestTokenUri[] = "https://accounts.google.com/o/oauth2/token";
const char kScope[] = "https://www.googleapis.com/auth/imesync";
const char kMachineIdKey[] = "oauth2.mid";
}

OAuth2Util::OAuth2Util(const OAuth2Client *client)
    : client_(client), authenticate_uri_(kAuthenticateUri),
      redirect_uri_(kRedirectUri), request_token_uri_(kRequestTokenUri),
      scope_(kScope) {}

OAuth2Util::~OAuth2Util() {}

string OAuth2Util::GetAuthenticateUri() {
  string uri;
  OAuth2::GetAuthorizeUri(authenticate_uri_, client_->client_id_, redirect_uri_,
                          scope_, "", &uri);
  return uri;
}

bool OAuth2Util::RequestAccessToken(const string &auth_token) {
  string access_token;
  string refresh_token;
  if (!OAuth2::AuthorizeToken(request_token_uri_, client_->client_id_,
                              client_->client_secret_, redirect_uri_,
                              auth_token, scope_, "", &access_token,
                              &refresh_token)) {
    LOG(ERROR) << "Authorization in " << authenticate_uri_ << " failed";
    return false;
  }
  if (!RegisterTokens(access_token, refresh_token)) {
    LOG(ERROR) << "Registration of Access Token and Refresh Token failed";
    return false;
  }
  return true;
}

bool OAuth2Util::RefreshAccessToken() {
  string access_token;
  string refresh_token;
  if (!GetTokens(&access_token, &refresh_token)) {
    return false;
  }
  if (!OAuth2::RefreshTokens(request_token_uri_, client_->client_id_,
                             client_->client_secret_, scope_, &refresh_token,
                             &access_token)) {
    LOG(ERROR) << "Refreshtokens failed";
    return false;
  }
  return RegisterTokens(access_token, refresh_token);
}

bool OAuth2Util::RequestResource(const string &resource_uri, string *resource) {
  string access_token;
  string refresh_token;
  if (!GetTokens(&access_token, &refresh_token)) {
    return false;
  }
  if (!OAuth2::GetProtectedResource(resource_uri, access_token, resource)) {
    LOG(ERROR) << "Cannot get resource from " << resource_uri;
    return false;
  }
  return true;
}

void OAuth2Util::Clear() {
  const string access_key(GetAccessKey());
  const string refresh_key(GetRefreshKey());
  if (!storage::Registry::Erase(access_key)) {
    LOG(WARNING) << "cannot erase key: " << access_key;
  }
  if (!storage::Registry::Erase(refresh_key)) {
    LOG(WARNING) << "cannot erase key: " << refresh_key;
  }
}

bool OAuth2Util::InitMID() {
  const size_t kMidSize = 64;
  char tmp[kMidSize + 1];
  Util::GetSecureRandomAsciiSequence(tmp, sizeof(tmp));
  tmp[kMidSize] = '\0';
  const string mid = tmp;
  if (!mozc::storage::Registry::Insert(kMachineIdKey, mid)) {
    LOG(ERROR) << "cannot insert to registry: " << kMachineIdKey;
    return false;
  }
  return true;
}

bool OAuth2Util::GetMID(string *mid) {
  if (storage::Registry::Lookup(kMachineIdKey, mid)) {
    return true;
  }

  LOG(WARNING) << "cannot find: " << kMachineIdKey;
  if (InitMID() && storage::Registry::Lookup(kMachineIdKey, mid)) {
    return true;
  }

  LOG(ERROR) << "cannot make/get MID";
  return false;
}

void OAuth2Util::set_scope(const string &scope) {
  scope_ = scope;
}

bool OAuth2Util::GetAccessToken(string *access_token) {
  string dummy_refresh_token;
  return GetTokens(access_token, &dummy_refresh_token);
}

bool OAuth2Util::GetTokens(string *access_token, string *refresh_token) {
  const string access_key(GetAccessKey());
  const string refresh_key(GetRefreshKey());

  string encrypted_token;
  if (!storage::Registry::Lookup(access_key, &encrypted_token)) {
    LOG(WARNING) << "cannot find: " << access_key;
    return false;
  }
  if (!DecryptString(encrypted_token, access_token)) {
    LOG(ERROR) << "Decryption Access Token failed";
    return false;
  }
  if (!storage::Registry::Lookup(refresh_key, refresh_token)) {
    LOG(WARNING) << "cannot find: " << refresh_key;
    return false;
  }

  return true;
}

bool OAuth2Util::RegisterTokens(const string &access_token,
                                const string &refresh_token) {
  const string access_key(GetAccessKey());
  const string refresh_key(GetRefreshKey());

  string encrypted_token;
  if (!EncryptString(access_token, &encrypted_token)) {
    LOG(ERROR) << "cannot encrypt Access Token";
    return false;
  }
  if (!storage::Registry::Insert(access_key, encrypted_token)) {
    LOG(ERROR) << "cannot regist Access Token";
    return false;
  }
  if (!storage::Registry::Insert(refresh_key, refresh_token)) {
    LOG(ERROR) << "cannot regist Refresh Token";
    return false;
  }
  // save registry data into a file
  if (!storage::Registry::Sync()) {
    LOG(WARNING) << "registered tokens are not saved yet";
  }

  return true;
}

bool OAuth2Util::EncryptString(const string &plain, string *crypt) {
  string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "PasswordManager::GetPassword() failed";
    return false;
  }
  if (password.empty()) {
    LOG(ERROR) << "password is empty";
    return false;
  }

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password)) {
    LOG(ERROR) << "Encryptor::Key::DeriveFromPassword() failed";
    return false;
  }

  *crypt = plain;
  if (!Encryptor::EncryptString(key, crypt)) {
    LOG(ERROR) << "Encryptor::EncryptString() failed";
    return false;
  }
  return true;
}

bool OAuth2Util::DecryptString(const string &crypt, string *plain) {
  string password;
  if (!PasswordManager::GetPassword(&password)) {
    LOG(ERROR) << "PasswordManager::GetPassword() failed";
    return false;
  }
  if (password.empty()) {
    LOG(ERROR) << "password is empty";
    return false;
  }

  Encryptor::Key key;
  if (!key.DeriveFromPassword(password)) {
    LOG(ERROR) << "Encryptor::Key::DeriveFromPassword() failed";
    return false;
  }

  *plain = crypt;
  if (!Encryptor::DecryptString(key, plain)) {
    LOG(ERROR) << "Encryptor::DecryptString() failed";
    return false;
  }
  return true;
}

string OAuth2Util::GetAccessKey() {
  return "oauth2." + client_->name_ + ".access_token";
}

string OAuth2Util::GetRefreshKey() {
  return "oauth2." + client_->name_ + ".refresh_token";
}

}  // namespace sync
}  // namespace mozc
