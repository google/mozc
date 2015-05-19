// Copyright 2010-2014, Google Inc.
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

// The test for Android's jni proxy class. This test should be built only
// for android.

#ifndef OS_ANDROID
#error "Should be built only for Android."
#endif  // OS_ANDROID

#include "base/android_jni_proxy.h"

#include "base/android_jni_mock.h"
#include "base/logging.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace jni {

namespace {

class MockJavaHttpClientImpl : public MockJavaHttpClient {
 public:
  MOCK_METHOD3(Request, jbyteArray(jbyteArray, jbyteArray, jbyteArray));
};

class AndroidJniProxyTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    jvm_.reset(new MockJavaVM);
  }

  virtual void TearDown() {
    jvm_.reset(NULL);
  }

  scoped_ptr<MockJavaVM> jvm_;
};

bool IsEqualByteArray(MockJNIEnv *env,
                      const jbyteArray& lhs, const jbyteArray& rhs) {
  jboolean dummy;
  jsize lhs_size = env->GetArrayLength(lhs);
  jsize rhs_size = env->GetArrayLength(rhs);
  jbyte *lhs_buf = env->GetByteArrayElements(lhs, &dummy);
  jbyte *rhs_buf = env->GetByteArrayElements(rhs, &dummy);
  if (lhs_size != rhs_size) {
    return false;
  }
  return memcmp(lhs_buf, rhs_buf, lhs_size) == 0;
}

class ByteArrayMatcher {
 public:
  ByteArrayMatcher(MockJNIEnv *env, const jbyteArray& lhs):
    env_(env), lhs_(lhs) {}
  bool operator() (const jbyteArray& rhs) const {
    return IsEqualByteArray(env_, lhs_, rhs);
  }
 private:
  MockJNIEnv *env_;
  const jbyteArray& lhs_;
};

}  // namespace


class HttpClientParams {
 public:
  const HTTPMethodType method_type_;
  const string method_;
  const string url_;
  const string post_data_;
  const string result_;
  HttpClientParams(HTTPMethodType method_type,
                   const string& method,
                   const string& url,
                   const string& post_data,
                   const string& result) :
    method_type_(method_type), method_(method), url_(url),
    post_data_(post_data), result_(result) {}
};

class AndroidJniProxyHttpClientTest
    : public AndroidJniProxyTest,
      public testing::WithParamInterface<HttpClientParams> {
 protected:
  virtual void SetUp() {
    AndroidJniProxyTest::SetUp();
    MockJNIEnv *env = jvm_->mutable_env();
    mock_http_client_ = new MockJavaHttpClientImpl();

    // env takes ownership of the mocks.
    env->RegisterMockJavaHttpClient(mock_http_client_);
    JavaHttpClientProxy::SetJavaVM(jvm_->mutable_jvm());
  }

  virtual void TearDown() {
    JavaHttpClientProxy::SetJavaVM(NULL);
    mock_http_client_ = NULL;
    AndroidJniProxyTest::TearDown();
  }

  MockJavaHttpClientImpl *mock_http_client_;
};

TEST_P(AndroidJniProxyHttpClientTest, Request) {
  MockJNIEnv *env = jvm_->mutable_env();
  const HttpClientParams &params = GetParam();
  const jbyteArray& method = env->StringToJByteArray(params.method_);
  const jbyteArray& url = env->StringToJByteArray(params.url_);
  const jbyteArray& post_data = env->StringToJByteArray(params.post_data_);
  const jbyteArray& result = env->StringToJByteArray(params.result_);
  EXPECT_CALL(*mock_http_client_,
              Request(testing::Truly(ByteArrayMatcher(env, method)),
                      testing::Truly(ByteArrayMatcher(env, url)),
                      testing::Truly(ByteArrayMatcher(env, post_data))))
      .WillOnce(testing::Return(result));
  string output_string;
  ASSERT_TRUE(JavaHttpClientProxy::Request(
      params.method_type_,
      params.url_,
      params.post_data_.c_str(),
      params.post_data_.length(),
      HTTPClient::Option(),
      &output_string));
  EXPECT_EQ(params.result_, output_string);
}

INSTANTIATE_TEST_CASE_P(
    AndroidJniProxyHttpClientTest,
    AndroidJniProxyHttpClientTest,
    ::testing::Values(
        HttpClientParams(mozc::HTTP_GET, "GET", "http://example.com",
                         "", "result1"),
        HttpClientParams(mozc::HTTP_HEAD, "HEAD", "http://example.com",
                         "", "result2"),
        HttpClientParams(mozc::HTTP_POST, "POST", "http://example.com",
                         "postdata", "result3")));
}  // namespace jni
}  // namespace mozc
