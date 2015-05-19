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

#include <string>

#include "base/port.h"
#include "base/util.h"
#include "base/number_util.h"
#include "base/pepper_file_util.h"
#include "chrome/nacl/dictionary_downloader.h"
#include "net/http_client.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace chrome {
namespace nacl {

class DictionaryDownloaderTest : public testing::Test {
 protected:
  DictionaryDownloaderTest()
      : base_url_("http://127.0.0.1:9999/RETRY_TEST") {
    // TODO(horo) Don't use the fixed port number.
  }
  virtual void SetUp() {
    output_.clear();
  }
  void SetRetryTestCounter(int32 counter) {
    // When we set the counter in the server a negative value, HTTP get method
    // to "/RETRY_TEST" returns 404.
    EXPECT_TRUE(HTTPClient::Get(
        base_url_ + "?action=set_counter&value=" +
        NumberUtil::SimpleItoa(counter),
        &output_));
    CheckRetryTestCounter(counter);
  }
  void CheckRetryTestCounter(int32 counter) {
    EXPECT_TRUE(HTTPClient::Get(
        base_url_ + "?action=get_counter", &output_));
    EXPECT_EQ(NumberUtil::SimpleItoa(counter), output_);
  }

  const string base_url_;
  string output_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DictionaryDownloaderTest);
};

TEST_F(DictionaryDownloaderTest, HTTPClientRetryTest) {
  SetRetryTestCounter(0);
  EXPECT_TRUE(HTTPClient::Get(base_url_, &output_));
  EXPECT_EQ("DEFAULT_DATA", output_);
  CheckRetryTestCounter(1);
  EXPECT_TRUE(HTTPClient::Get(base_url_, &output_));
  EXPECT_EQ("DEFAULT_DATA", output_);
  CheckRetryTestCounter(2);

  SetRetryTestCounter(-2);
  EXPECT_FALSE(HTTPClient::Get(base_url_, &output_));
  CheckRetryTestCounter(-1);
  EXPECT_FALSE(HTTPClient::Get(base_url_, &output_));
  CheckRetryTestCounter(0);
  EXPECT_TRUE(HTTPClient::Get(base_url_, &output_));
  EXPECT_EQ("DEFAULT_DATA", output_);
  CheckRetryTestCounter(1);
}

TEST_F(DictionaryDownloaderTest, SimpleTest) {
  SetRetryTestCounter(0);
  scoped_ptr<DictionaryDownloader> downloader;
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_FINISHED) {
    Util::Sleep(10);
  }
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("DEFAULT_DATA", output_);
  downloader.reset(new DictionaryDownloader(base_url_ + "?data=0123456789",
                                            "/test01"));
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_FINISHED) {
    Util::Sleep(10);
  }
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("0123456789", output_);
  CheckRetryTestCounter(2);
}

TEST_F(DictionaryDownloaderTest, LargeDataTest) {
  SetRetryTestCounter(0);
  scoped_ptr<DictionaryDownloader> downloader;
  downloader.reset(new DictionaryDownloader(
      base_url_ + "?data=0123456789&times=1000000",
      "/large_data"));
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_FINISHED) {
    Util::Sleep(100);
  }
  PepperFileUtil::ReadBinaryFile("/large_data", &output_);
  ASSERT_EQ(10000000, output_.size());
  for (size_t i = 0; i < 1000000; ++i) {
    EXPECT_EQ(0, memcmp("0123456789", output_.data() + i * 10, 10));
  }
}

TEST_F(DictionaryDownloaderTest, RetryTest) {
  SetRetryTestCounter(-1);
  scoped_ptr<DictionaryDownloader> downloader;
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_ERROR) {
    Util::Sleep(10);
  }

  SetRetryTestCounter(-2);
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(0, 0, 0, 0, 1);
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_ERROR) {
    Util::Sleep(10);
  }
  CheckRetryTestCounter(0);

  SetRetryTestCounter(-2);
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(0, 0, 0, 0, 2);
  downloader->StartDownload();
  while (downloader->GetStatus() != DictionaryDownloader::DOWNLOAD_FINISHED) {
    Util::Sleep(10);
  }
  CheckRetryTestCounter(1);
}

TEST_F(DictionaryDownloaderTest, DelayTest) {
  SetRetryTestCounter(0);
  scoped_ptr<DictionaryDownloader> downloader;
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(1000, 0, 0, 0, 0);
  downloader->StartDownload();
  Util::Sleep(500);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_PENDING, downloader->GetStatus());
  Util::Sleep(1000);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_FINISHED, downloader->GetStatus());
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("DEFAULT_DATA", output_);
}


TEST_F(DictionaryDownloaderTest, RetryIntervalTest) {
  SetRetryTestCounter(-3);
  scoped_ptr<DictionaryDownloader> downloader;
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(0, 0, 1000, 0, 3);
  downloader->StartDownload();
  Util::Sleep(500);
  CheckRetryTestCounter(-2);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(-1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_FINISHED, downloader->GetStatus());
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("DEFAULT_DATA", output_);
}

TEST_F(DictionaryDownloaderTest, RetryIntervalBackOffTest) {
  scoped_ptr<DictionaryDownloader> downloader;
  SetRetryTestCounter(-3);
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(0, 0, 1000, 1, 3);
  downloader->StartDownload();
  Util::Sleep(500);
  CheckRetryTestCounter(-2);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(-1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(-1);
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  Util::Sleep(1000);
  CheckRetryTestCounter(1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_FINISHED, downloader->GetStatus());
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("DEFAULT_DATA", output_);

  SetRetryTestCounter(-3);
  downloader.reset(new DictionaryDownloader(base_url_, "/test01"));
  downloader->SetOption(0, 0, 1000, 2, 3);
  downloader->StartDownload();
  Util::Sleep(500);
  CheckRetryTestCounter(-2);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(-1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(-1);
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_WAITING_FOR_RETRY,
            downloader->GetStatus());
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  Util::Sleep(1000);
  CheckRetryTestCounter(0);
  Util::Sleep(1000);
  CheckRetryTestCounter(1);
  EXPECT_EQ(DictionaryDownloader::DOWNLOAD_FINISHED, downloader->GetStatus());
  PepperFileUtil::ReadBinaryFile("/test01", &output_);
  EXPECT_EQ("DEFAULT_DATA", output_);
}

}   // namespace nacl
}   // namespace chrome

class PepperHTTPClientTest : public testing::Test {
 protected:
  PepperHTTPClientTest()
      : base_url_("http://127.0.0.1:9999/test") {
    // TODO(horo) Don't use the fixed port number.
  }
  virtual void SetUp() {
    output_.clear();
    option_ = HTTPClient::Option();
  }
  const string base_url_;
  string output_;
  HTTPClient::Option option_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PepperHTTPClientTest);
};

TEST_F(PepperHTTPClientTest, GetNormal) {
  EXPECT_TRUE(HTTPClient::Get(base_url_, &output_));
  EXPECT_EQ("DEFAULT_DATA", output_);
}

TEST_F(PepperHTTPClientTest, GetQuery) {
  EXPECT_TRUE(HTTPClient::Get(base_url_ + "?data=foobar", &output_));
  EXPECT_EQ("foobar", output_);
}

TEST_F(PepperHTTPClientTest, GetNotFound) {
  EXPECT_FALSE(HTTPClient::Get(base_url_ + "?response=404", &output_));
}

TEST_F(PepperHTTPClientTest, GetRedirect) {
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?response=301&data=aaa&redirect_location=/test%3Fdata=bbb",
      &output_));
  EXPECT_EQ("bbb", output_);
}

TEST_F(PepperHTTPClientTest, GetResponseHeader) {
  option_.include_header = true;
  EXPECT_TRUE(HTTPClient::Get(base_url_ + "?data=foobar", option_, &output_));
  EXPECT_NE(string::npos,
            output_.find("\ncommand: GET\n"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test\n"));
  EXPECT_EQ("\n\nfoobar", output_.substr(output_.size() - 8));
}

TEST_F(PepperHTTPClientTest, GetRequestHeader) {
  option_.include_header = true;
  option_.headers.push_back("Test-Header1: TestData1");
  option_.headers.push_back("Test-Header2: TestData2");
  option_.headers.push_back("Content-Type: TestContentType");
  // We can't set the custom agent in Chrome NaCl environment.
  option_.headers.push_back("User-Agent: Test Browser");
  EXPECT_TRUE(HTTPClient::Get(base_url_ + "?data=foobar", option_, &output_));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header1: TestData1"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header2: TestData2"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_content-type: TestContentType"));
  EXPECT_EQ(string::npos,
            output_.find("Test Browser"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test\n"));
  EXPECT_EQ("\n\nfoobar", output_.substr(output_.size() - 8));
}

TEST_F(PepperHTTPClientTest, GetContentLength) {
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789&content_length=10000",
      option_,
      &output_));
  EXPECT_EQ(10000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetContentLengthBiggerThanMaxDataSizeOK) {
  option_.max_data_size = 10000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789&content_length=10000",
      option_,
      &output_));
  EXPECT_EQ(10000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetContentLengthBiggerThanMaxDataSizeNG) {
  option_.max_data_size = 9999;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789&content_length=10000",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetContentLengthAndHeaderBiggerThanMaxDataSize) {
  option_.include_header = true;
  option_.max_data_size = 10000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789&content_length=10000",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetContentLengthMissMatch) {
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789&content_length=10001",
      &output_));
}

TEST_F(PepperHTTPClientTest, GetMaxDataSizeOK) {
  option_.max_data_size = 10000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(10000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetMaxDataSizeNG) {
  option_.max_data_size = 9999;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?times=1000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetBeforeResponseSleepOK) {
  option_.timeout = 1000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?before_response_sleep=0.5&times=2000&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(20000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetBeforeResponseSleepNG) {
  option_.timeout = 1000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?before_response_sleep=1.5&times=2000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetBeforeHeadSleepOK) {
  option_.timeout = 1000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?before_head_sleep=0.5&times=2000&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(20000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetBeforeHeadSleepNG) {
  option_.timeout = 1000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?before_head_sleep=1.5&times=2000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetAfterHeadSleepOK) {
  option_.timeout = 1000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?after_head_sleep=0.5&times=2000&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(20000, output_.size());
}

TEST_F(PepperHTTPClientTest, GetAfterHeadSleepNG) {
  option_.timeout = 1000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?after_head_sleep=1.5&times=2000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetBeforeDataSleepOK) {
  option_.timeout = 1000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?before_data_sleep=0.01&times=10&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(100, output_.size());
}

TEST_F(PepperHTTPClientTest, GetBeforeDataSleepNG) {
  option_.timeout = 1000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?before_data_sleep=0.001&times=2000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, GetAfterDataSleepOK) {
  option_.timeout = 1000;
  EXPECT_TRUE(HTTPClient::Get(
      base_url_ + "?after_data_sleep=0.01&times=10&data=0123456789",
      option_,
      &output_));
  EXPECT_EQ(100, output_.size());
}

TEST_F(PepperHTTPClientTest, GetAfterDataSleepNG) {
  option_.timeout = 1000;
  EXPECT_FALSE(HTTPClient::Get(
      base_url_ + "?after_data_sleep=0.001&times=2000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostNormal) {
  EXPECT_TRUE(HTTPClient::Post(base_url_, "foobar", &output_));
  EXPECT_EQ("foobar", output_);
}

TEST_F(PepperHTTPClientTest, PostNotFound) {
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?response=404", "foobar", &output_));
}

TEST_F(PepperHTTPClientTest, PostRedirect) {
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?response=301&redirect_location=/test%3Fdata=bbb",
      "aaa",
      &output_));
  EXPECT_EQ("bbb", output_);
}

TEST_F(PepperHTTPClientTest, PostResponseHeader) {
  option_.include_header = true;
  EXPECT_TRUE(HTTPClient::Post(base_url_, "foobar", option_, &output_));
  EXPECT_NE(string::npos,
            output_.find("\ncommand: POST\n"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test\n"));
  EXPECT_EQ("\n\nfoobar", output_.substr(output_.size() - 8));
}

TEST_F(PepperHTTPClientTest, PostRequestHeader) {
  option_.include_header = true;
  option_.headers.push_back("Test-Header1: TestData1");
  option_.headers.push_back("Test-Header2: TestData2");
  option_.headers.push_back("Content-Type: TestContentType");
  // We can't set the custom agent in Chrome NaCl environment.
  option_.headers.push_back("User-Agent: Test Browser");
  EXPECT_TRUE(HTTPClient::Post(base_url_, "foobar", option_, &output_));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header1: TestData1"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header2: TestData2"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_content-type: TestContentType"));
  EXPECT_EQ(string::npos,
            output_.find("Test Browser"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test\n"));
  EXPECT_EQ("\n\nfoobar", output_.substr(output_.size() - 8));
}

TEST_F(PepperHTTPClientTest, PostContentLength) {
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?content_length=10000",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostContentLengthBiggerThanMaxDataSizeOK) {
  option_.max_data_size = 10000;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?content_length=10000",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostContentLengthBiggerThanMaxDataSizeNG) {
  option_.max_data_size = 9999;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?content_length=10000",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostContentLengthAndHeaderBiggerThanMaxDataSize) {
  option_.include_header = true;
  option_.max_data_size = 10000;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?content_length=10000",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostContentLengthMissMatch) {
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?content_length=10001",
      string(10000, ' '),
      &output_));
}

TEST_F(PepperHTTPClientTest, PostMaxDataSizeOK) {
  option_.max_data_size = 10000;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_,
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostMaxDataSizeNG) {
  option_.max_data_size = 9999;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_,
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostBeforeResponseSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?before_response_sleep=0.1",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostBeforeResponseSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?before_response_sleep=0.5",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostBeforeHeadSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?before_head_sleep=0.1",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostBeforeHeadSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?before_head_sleep=0.5",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostAfterHeadSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?after_head_sleep=0.1",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostAfterHeadSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?after_head_sleep=0.5",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, PostAfterDataSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Post(
      base_url_ + "?after_data_sleep=0.1",
      string(10000, ' '),
      option_,
      &output_));
  EXPECT_EQ(string(10000, ' '), output_);
}

TEST_F(PepperHTTPClientTest, PostAfterDataSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Post(
      base_url_ + "?after_data_sleep=0.5",
      string(10000, ' '),
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, HeadNormal) {
  EXPECT_TRUE(HTTPClient::Head(base_url_, &output_));
  EXPECT_NE(string::npos,
            output_.find("\ncommand: HEAD\n"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test"));
}

TEST_F(PepperHTTPClientTest, HeadNotFound) {
  EXPECT_FALSE(HTTPClient::Head(base_url_ + "?response=404", &output_));
}

TEST_F(PepperHTTPClientTest, HeadRequestHeader) {
  option_.headers.push_back("Test-Header1: TestData1");
  option_.headers.push_back("Test-Header2: TestData2");
  option_.headers.push_back("Content-Type: TestContentType");
  // We can't set the custom agent in Chrome NaCl environment.
  option_.headers.push_back("User-Agent: Test Browser");
  EXPECT_TRUE(HTTPClient::Head(base_url_ + "?data=foobar", option_, &output_));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header1: TestData1"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_test-header2: TestData2"));
  EXPECT_NE(string::npos,
            output_.find("CLIENT_HEADER_content-type: TestContentType"));
  EXPECT_EQ(string::npos,
            output_.find("Test Browser"));
  EXPECT_NE(string::npos,
            output_.find("\nparsed_path: /test"));
}

TEST_F(PepperHTTPClientTest, HeadMaxDataSizeNG) {
  option_.max_data_size = 30;
  EXPECT_FALSE(HTTPClient::Head(
      base_url_ + "?times=1000&data=0123456789",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, HeadBeforeResponseSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Head(
      base_url_ + "?before_response_sleep=0.1",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, HeadBeforeResponseSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Head(
      base_url_ + "?before_response_sleep=0.5",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, HeadBeforeHeadSleepOK) {
  option_.timeout = 300;
  EXPECT_TRUE(HTTPClient::Head(
      base_url_ + "?before_head_sleep=0.1",
      option_,
      &output_));
}

TEST_F(PepperHTTPClientTest, HeadBeforeHeadSleepNG) {
  option_.timeout = 300;
  EXPECT_FALSE(HTTPClient::Head(
      base_url_ + "?before_head_sleep=0.5",
      option_,
      &output_));
}

}  // namespace mozc
