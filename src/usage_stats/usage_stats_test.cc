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

#include "usage_stats/usage_stats.h"

#ifdef OS_WINDOWS
#include <time.h>  // time()
#else
#include <sys/time.h>  // time()
#endif

#include <algorithm>
#include <string>
#include <vector>

#include "base/util.h"
#include "base/version.h"
#include "base/singleton.h"
#include "base/stats_config_util.h"
#include "net/http_client.h"
#include "storage/registry.h"
#include "storage/tiny_storage.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace usage_stats {
namespace {
class TestHTTPClient : public HTTPClientInterface {
 public:
  bool Get(const string &url, string *output) const { return true; }
  bool Head(const string &url, string *output) const { return true; }
  bool Post(const string &url, const string &data,
            string *output) const { return true; }

  bool Get(const string &url, const HTTPClient::Option &option,
           string *output) const { return true; }
  bool Head(const string &url, const HTTPClient::Option &option,
            string *output) const { return true; }
  bool Post(const string &url, const char *data,
            size_t data_size, const HTTPClient::Option &option,
            string *output) const { return true; }

  bool Get(const string &url, ostream *output)  const { return true; }
  bool Head(const string &url, ostream *output)  const { return true; }
  bool Post(const string &url, const string &data,
            ostream *output) const { return true; }

  bool Get(const string &url, const HTTPClient::Option &option,
           ostream *output)  const { return true; }
  bool Head(const string &url, const HTTPClient::Option &option,
            ostream *output)  const { return true; }
  bool Post(const string &url, const string &data,
            const HTTPClient::Option & option,
            std::ostream *outptu) const { return true; }
  bool Post(const string &url, const char *data,
            size_t data_size, const HTTPClient::Option &option,
            std::ostream *output) const { return true; }

  bool Post(const string &url, const string &data,
            const HTTPClient::Option &option, string *output) const {
    LOG(INFO) << "url: " << url;
    LOG(INFO) << "data: " << data;
    if (result_.expected_url != url) {
      LOG(INFO) << "expected_url: " << result_.expected_url;
      return false;
    }

    vector<string> data_set;
    Util::SplitStringUsing(data, "&", &data_set);
    for (size_t i = 0; i < expected_data_.size(); ++i) {
      vector<string>::const_iterator itr =
          find(data_set.begin(), data_set.end(), expected_data_[i]);
      const bool found = (itr != data_set.end());
      // we can't compile EXPECT_NE(itr, data_set.end()), so we use EXPECT_TRUE
      EXPECT_TRUE(found) << expected_data_[i];
    }

    *output = result_.expected_result;
    return true;
  }

  struct Result {
    string expected_url;
    string expected_result;
  };

  void set_result(const Result &result) {
    result_ = result;
  }

  // TODO(toshiyuki): integrate with struct Result
  void AddExpectedData(const string &data) {
    expected_data_.push_back(data);
  }

 private:
  // usage stats key and value parameter
  // format is "<key>:<type>=<value>"
  vector<string> expected_data_;
  Result result_;
};

class TestStatsConfigUtil : public StatsConfigUtilInterface {
 public:
  TestStatsConfigUtil() {
    val_ = true;
  }

  virtual ~TestStatsConfigUtil() {}

  virtual bool IsEnabled() {
    return val_;
  }

  virtual bool SetEnabled(bool val) {
    val_ = val;
    return true;
  }

 private:
  bool val_;
};

const char kBaseUrl[] = "http://clients4.google.com/tbproxy/usagestats";

const char kTestClientId[] = "TestClientId";

class TestClientId : public ClientIdInterface {
 public:
  TestClientId() {};
  virtual ~TestClientId() {};
  void GetClientId(string *output) {
    *output = kTestClientId;
  }
};
}  // namespace

class UsageStatsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    UsageStats::SetClientIdHandler(&client_id_);
    stats_config_util_.SetEnabled(true);
    StatsConfigUtil::SetHandler(&stats_config_util_);
    EXPECT_TRUE(StatsConfigUtil::IsEnabled());

    HTTPClient::SetHTTPClientHandler(&client_);
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    EXPECT_TRUE(storage::Registry::Clear());

    // save count test stats
    Stats stats;
    stats.set_name("Commit");
    stats.set_type(Stats::COUNT);
    stats.set_count(100);
    string stats_str;
    stats.AppendToString(&stats_str);
    EXPECT_TRUE(storage::Registry::Insert("usage_stats.Commit",
                                        stats_str));

    // integer test stats
    stats.set_name("ConfigHistoryLearningLevel");
    stats.set_type(Stats::INTEGER);
    stats.set_int_value(0);
    stats_str.clear();
    stats.AppendToString(&stats_str);
    EXPECT_TRUE(storage::Registry::Insert(
        "usage_stats.ConfigHistoryLearningLevel", stats_str));
  }

  void SetValidResult() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("sourceid", "ime"));
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", Version::GetMozcVersion()));
    params.push_back(make_pair("client_id", kTestClientId));
    params.push_back(make_pair("os_ver", Util::GetOSVersionString()));

    string url = string(kBaseUrl) + "?";
    Util::AppendCGIParams(params, &url);
    TestHTTPClient::Result result;
    result.expected_url = url;
    client_.set_result(result);
  }

  TestHTTPClient client_;
  TestClientId client_id_;
  TestStatsConfigUtil stats_config_util_;
};

TEST_F(UsageStatsTest, SendTest) {
  // save last_upload
  const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kOneDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();
  const uint32 send_sec = time(NULL);
  EXPECT_TRUE(UsageStats::Send(NULL));

  string stats_str;
  // COUNT stats are cleared
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  // INTEGER stats are not cleared
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_GE(recorded_sec, send_sec);
}

TEST_F(UsageStatsTest, SendFailTest) {
  // save last_upload
  const uint32 kHalfDaySec = 12 * 60 * 60;  // 12 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kHalfDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();
  EXPECT_FALSE(UsageStats::Send(NULL));

  string stats_str;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_EQ(recorded_sec, yesterday_sec);
}

TEST_F(UsageStatsTest, InvalidLastUploadTest) {
  // save last_upload
  const uint32 kHalfDaySec = 12 * 60 * 60;  // 12 hours
  const uint32 current_sec = time(NULL);
  // future time
  // for example: time zone has changed
  const uint32 invalid_sec = current_sec + kHalfDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        invalid_sec));
  SetValidResult();
  EXPECT_TRUE(UsageStats::Send(NULL));

  string stats_str;
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // Save new last_upload_time
  EXPECT_GE(recorded_sec, current_sec);
}

namespace {
class TestStorage: public storage::StorageInterface {
 public:
  bool Open(const string &filename) { return true; }
  bool Sync() { return true; }
  bool Lookup(const string &key, string *value) const { return false; }
  // return false
  bool Insert(const string &key, const string &value) { return false; }
  bool Erase(const string &key) { return true; }
  bool Clear() { return true; }
  size_t Size() const { return 0; }
  TestStorage() {}
  virtual ~TestStorage() {}
};
}  // namespace

TEST_F(UsageStatsTest, SaveCurrentTimeFailTest) {
  // save last_upload
  const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kOneDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();

  // set the TestStorage as a storage handler.
  // writing to the registry will be failed.
  storage::Registry::SetStorage(Singleton<TestStorage>::get());
  // confirm that we can not insert.
  EXPECT_FALSE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));

  EXPECT_FALSE(UsageStats::Send(NULL));
  // restore
  storage::Registry::SetStorage(NULL);

  // stats data are kept
  string stats_str;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_EQ(recorded_sec, yesterday_sec);
}

TEST_F(UsageStatsTest, UploadFailTest) {
  // save last_upload
  const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kOneDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();

  TestHTTPClient::Result result;
  // set dummy result url so that upload will be failed.
  result.expected_url = "fail_url";
  client_.set_result(result);

  EXPECT_FALSE(UsageStats::Send(NULL));

  // stats data are not cleared
  string stats_str;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  // "UsageStatsUploadFailed" is incremented
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.UsageStatsUploadFailed",
                                        &stats_str));
  Stats stats;
  EXPECT_TRUE(stats.ParseFromString(stats_str));
  EXPECT_EQ(Stats::COUNT, stats.type());
  EXPECT_EQ(1, stats.count());
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // last_upload is not updated
  EXPECT_EQ(recorded_sec, yesterday_sec);
}

TEST_F(UsageStatsTest, UploadRetryTest) {
  // save last_upload
  const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kOneDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();

  TestHTTPClient::Result result;
  // set dummy result url so that upload will be failed.
  result.expected_url = "fail_url";
  client_.set_result(result);

  EXPECT_FALSE(UsageStats::Send(NULL));

  // stats data are not cleared
  string stats_str;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // last_upload is not updated
  EXPECT_EQ(recorded_sec, yesterday_sec);

  // retry
  SetValidResult();
  // We can send stats if network is available.
  EXPECT_TRUE(UsageStats::Send(NULL));

  // Stats are cleared
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.Commit", &stats_str));
  // However, INTEGER stats are not cleared
  EXPECT_TRUE(storage::Registry::Lookup(
      "usage_stats.ConfigHistoryLearningLevel", &stats_str));
  // last upload is updated
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_GT(recorded_sec, yesterday_sec);
}

// for windows only
TEST_F(UsageStatsTest, GetMsctfVerTest) {
#ifdef OS_WINDOWS
  int major, minor, build, revision;
  const wchar_t kDllName[] = L"msctf.dll";
  wstring path = Util::GetSystemDir();
  path += L"\\";
  path += kDllName;
  EXPECT_TRUE(Util::GetFileVersion(path, &major, &minor, &build, &revision));
#endif
}

TEST_F(UsageStatsTest, UploadDataTest) {
  // save last_upload
  const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
  const uint32 current_sec = time(NULL);
  const uint32 yesterday_sec = current_sec - kOneDaySec;
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        yesterday_sec));
  SetValidResult();

#ifdef OS_WINDOWS
  const string win64 = (string("WindowsX64:b=")
                        + (Util::IsWindowsX64()? "t" : "f"));
  client_.AddExpectedData(win64);
  int major, minor, build, revision;
  const wchar_t kDllName[] = L"msctf.dll";
  wstring path = Util::GetSystemDir();
  path += L"\\";
  path += kDllName;
  if (Util::GetFileVersion(path, &major, &minor, &build, &revision)) {
    client_.AddExpectedData(
        string("MsctfVerMajor:i=") + Util::SimpleItoa(major));
    client_.AddExpectedData(
        string("MsctfVerMinor:i=") + Util::SimpleItoa(minor));
    client_.AddExpectedData(
        string("MsctfVerBuild:i=") + Util::SimpleItoa(build));
    client_.AddExpectedData(
        string("MsctfVerRevision:i=") + Util::SimpleItoa(revision));
  } else {
    LOG(ERROR) << "get file version for msctf.dll failed";
  }
#endif
  client_.AddExpectedData(string("Commit:c=100"));
  client_.AddExpectedData(string("ConfigHistoryLearningLevel:i=0"));
  client_.AddExpectedData(string("Daily"));

  EXPECT_TRUE(UsageStats::Send(NULL));
}

TEST(ClientIdTest, CreateClientIdTest) {
  // test default client id handler here
  UsageStats::SetClientIdHandler(NULL);
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  UsageStats::GetClientId(&client_id1);
  string client_id_in_storage1;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage1));
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id2;
  UsageStats::GetClientId(&client_id2);
  string client_id_in_storage2;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage2));

  EXPECT_NE(client_id1, client_id2);
  EXPECT_NE(client_id_in_storage1, client_id_in_storage2);
}

TEST(ClientIdTest, GetClientIdTest) {
  // test default client id handler here.
  UsageStats::SetClientIdHandler(NULL);
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  UsageStats::GetClientId(&client_id1);
  string client_id2;
  UsageStats::GetClientId(&client_id2);
  // we can get same client id.
  EXPECT_EQ(client_id1, client_id2);

  string client_id_in_storage;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage));
  // encrypted value is in storage
  EXPECT_NE(client_id1, client_id_in_storage);
}

TEST(ClientIdTest, GetClientIdFailTest) {
  // test default client id handler here.
  UsageStats::SetClientIdHandler(NULL);
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  UsageStats::GetClientId(&client_id1);
  // insert invalid data
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.client_id",
                                        "invalid_data"));

  string client_id2;
  // decript should be failed
  UsageStats::GetClientId(&client_id2);
  // new id should be generated
  EXPECT_NE(client_id1, client_id2);
}
}  // namespace mozc::usage_stats
}  // namespace mozc
