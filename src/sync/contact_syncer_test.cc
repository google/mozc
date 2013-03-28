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

#include "sync/contact_syncer.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "net/http_client.h"
#include "net/http_client_mock.h"
#include "storage/memory_storage.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "sync/oauth2.h"
#include "sync/oauth2_client.h"
#include "sync/oauth2_server.h"
#include "sync/oauth2_util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace sync {

using config::Config;
using config::ConfigHandler;
using config::SyncConfig;

namespace {

const char kAuthorizeClientUrl[] = "https://accounts.google.com/o/oauth2/auth";
const char kRedirectUri[] = "urn:ietf:wg:oauth:2.0:oob";
const char kAuthorizeTokenUri[] = "https://accounts.google.com/o/oauth2/token";
const char kScope[] = "https://www.google.com/m8/feeds/";

const char kAuthToken[] = "4/correct_authorization_token";
const char kAccessToken[] = "1/first_correct_access_token_bbbbbbbbbbbbbbbb";
const char kRefreshToken[] = "1/first_correct_refresh_token_ccccccccccccccc";
const char kAccessToken2[] = "1/second_correct_access_token_bbbbbbbbbbbbbbb";
const char kRefreshToken2[] = "1/second_correct_refresh_token_cccccccccccccc";
const char kResourceUri[] =
    "https://www.google.com/m8/feeds/contacts/default/full";

}  // namespace

class ContactSyncerTest : public testing::Test {
 protected:
  ContactSyncerTest()
      : oauth2_client_("google", "dummyclientid", "dummyclientsecret"),
        oauth2_server_(
            kAuthorizeClientUrl, kRedirectUri, kAuthorizeTokenUri, kScope) {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    HTTPClient::SetHTTPClientHandler(&client_);
    ConfigHandler::SetConfigFileName("memory://config");

    storage_.reset(mozc::storage::MemoryStorage::New());
    mozc::storage::Registry::SetStorage(storage_.get());

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    SyncConfig *sync_config = config.mutable_sync_config();
    sync_config->set_use_config_sync(true);
    sync_config->set_use_user_dictionary_sync(true);
    sync_config->set_use_user_history_sync(true);
    sync_config->set_use_contact_list_sync(true);
    sync_config->set_use_learning_preference_sync(true);
    ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    HTTPClient::SetHTTPClientHandler(NULL);
    mozc::storage::Registry::SetStorage(NULL);
    storage_.reset();

    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    ConfigHandler::SetConfig(config);
  }

  void SetAuthorizationServer() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("grant_type", "authorization_code"));
    params.push_back(make_pair("client_id", GetClient().client_id_));
    params.push_back(make_pair("client_secret", GetClient().client_secret_));
    params.push_back(make_pair("redirect_uri", GetServer().redirect_uri_));
    params.push_back(make_pair("code", kAuthToken));
    params.push_back(make_pair("scope", GetServer().scope_));

    HTTPClientMock::Result result;
    result.expected_url = GetServer().request_token_uri_;
    Util::AppendCGIParams(params, &result.expected_request);
    result.expected_result = string("{") +
        "\"access_token\":\"" + kAccessToken + "\",\"token_type\":\"Bearer\","
        "\"expires_in\":3600,\"refresh_token\":\"" + kRefreshToken + "\"}";
    client_.set_result(result);
  }

  void SetResourceServer() {
    HTTPClientMock::Result result;
    result.expected_url = string(kResourceUri) + "?alt=json&v=3%2E0&"
        "max-results=999999&updated-min=2011%2D05%2D22T04%3A00%3A00%2E000Z";
    result.expected_result = "{\"feed\":{\"entry\":["
        "{\"gd$name\":{"
        "\"gd$familyName\":{"
        // "苗字"
        "\"$t\":\"\xE8\x8B\x97\xE5\xAD\x97\","
        // "みょうじ"
        "\"yomi\":\"\xE3\x81\xBF\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98\"},"
        "\"gd$fullName\":"
        // "苗字名前"
        "{\"$t\":\"\xE8\x8B\x97\xE5\xAD\x97\xE5\x90\x8D\xE5\x89\x8D\"},"
        "\"gd$givenName\":{"
        // "名前"
        "\"$t\":\"\xE5\x90\x8D\xE5\x89\x8D\","
        // "なまえ"
        "\"yomi\":\"\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88\""
        "}}}"
        "]}}";
    HTTPClient::Option option;
    option.include_header = true;
    option.headers.push_back(string("Authorization: OAuth ") + kAccessToken);
    client_.set_result(result);
    client_.set_option(option);
  }

  const OAuth2Client &GetClient() const {
    return oauth2_client_;
  }
  const OAuth2Server &GetServer() const {
    return oauth2_server_;
  }

 private:
  const OAuth2Client oauth2_client_;
  const OAuth2Server oauth2_server_;
  HTTPClientMock client_;
  scoped_ptr<mozc::storage::StorageInterface> storage_;
};

TEST_F(ContactSyncerTest, Timestamp) {
  SetAuthorizationServer();
  OAuth2Util oauth2_util(GetClient(), GetServer());
  oauth2_util.set_scope(GetServer().scope_);
  ContactSyncer syncer(&oauth2_util);

  string timestamp;
  EXPECT_FALSE(syncer.GetLastDownloadTimestamp(&timestamp));
  EXPECT_EQ("0000-00-00T00:00:00.000Z", timestamp);

  syncer.SetLastDownloadTimestamp("2011-05-22T04:00:00.000Z");
  EXPECT_TRUE(syncer.GetLastDownloadTimestamp(&timestamp));
  EXPECT_EQ("2011-05-22T04:00:00.000Z", timestamp);
}

TEST_F(ContactSyncerTest, Download) {
  SetAuthorizationServer();
  OAuth2Util oauth2_util(GetClient(), GetServer());
  oauth2_util.set_scope(GetServer().scope_);
  ContactSyncer syncer(&oauth2_util);

  EXPECT_EQ(OAuth2::kNone, oauth2_util.RequestAccessToken(kAuthToken));

  SetResourceServer();
  syncer.SetLastDownloadTimestamp("2011-05-22T04:00:00.000Z");

  bool reload_required = false;
  user_dictionary::UserDictionaryStorage storage;
  EXPECT_TRUE(syncer.Download(&storage, &reload_required));
  EXPECT_EQ(1, storage.dictionaries_size());
  user_dictionary::UserDictionary *user_dict = storage.mutable_dictionaries(0);
  EXPECT_EQ(1, user_dict->entries_size());
  user_dictionary::UserDictionary::Entry *entry =
      user_dict->mutable_entries(0);
  // "みょうじ""なまえ"
  EXPECT_EQ("\xE3\x81\xBF\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98"
            "\xE3\x81\xAA\xE3\x81\xBE\xE3\x81\x88", entry->key());
  // "苗字名前"
  EXPECT_EQ("\xE8\x8B\x97\xE5\xAD\x97\xE5\x90\x8D\xE5\x89\x8D", entry->value());
}

}  // namespace sync
}  // namespace mozc
