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

// We'd probably want to replace this class by gmock, but it has not been
// well tested on android yet. To split the work to add jni's proxy from
// introducing gmock, we manually mock JavaEncryptor temporarily.
// TODO(hidehiko): replace this class by gmock.
class MockJavaEncryptorImpl : public MockJavaEncryptor {
 public:
  MockJavaEncryptorImpl(MockJNIEnv *env) : env_(env) {
  }

  virtual jbyteArray DeriveFromPassword(jbyteArray password, jbyteArray salt) {
    CHECK(expected_derive_from_password_.get() != NULL);
    CHECK_EQ(expected_derive_from_password_->expected_password(),
             env_->JByteArrayToString(password));
    CHECK_EQ(expected_derive_from_password_->expected_salt(),
             env_->JByteArrayToString(salt));
    jbyteArray result = env_->StringToJByteArray(
        expected_derive_from_password_->expected_result());
    expected_derive_from_password_.reset();
    return result;
  }

  virtual jbyteArray Encrypt(jbyteArray data, jbyteArray key, jbyteArray iv) {
    CHECK(expected_encrypt_.get() != NULL);
    CHECK_EQ(expected_encrypt_->expected_data(),
             env_->JByteArrayToString(data));
    CHECK_EQ(expected_encrypt_->expected_key(), env_->JByteArrayToString(key));
    CHECK_EQ(expected_encrypt_->expected_iv(), env_->JByteArrayToString(iv));
    jbyteArray result =
        env_->StringToJByteArray(expected_encrypt_->expected_result());
    expected_encrypt_.reset();
    return result;
  }

  virtual jbyteArray Decrypt(jbyteArray data, jbyteArray key, jbyteArray iv) {
    CHECK(expected_decrypt_.get() != NULL);
    CHECK_EQ(expected_decrypt_->expected_data(),
             env_->JByteArrayToString(data));
    CHECK_EQ(expected_decrypt_->expected_key(), env_->JByteArrayToString(key));
    CHECK_EQ(expected_decrypt_->expected_iv(), env_->JByteArrayToString(iv));
    jbyteArray result =
        env_->StringToJByteArray(expected_decrypt_->expected_result());
    expected_decrypt_.reset();
    return result;
  }

  void ExpectDeriveFromPassword(
      const string &password, const string &salt, const string &result) {
    expected_derive_from_password_.reset(
        new ExpectedDeriveFromPassword(password, salt, result));
  }

  void ExpectEncrypt(
      const string &data, const string &key, const string &iv,
      const string &result) {
    expected_encrypt_.reset(new ExpectedCrypt(data, key, iv, result));
  }

  void ExpectDecrypt(
      const string &data, const string &key, const string &iv,
      const string &result) {
    expected_decrypt_.reset(new ExpectedCrypt(data, key, iv, result));
  }

  bool has_expected_derive_from_password() const {
    return expected_derive_from_password_.get() != NULL;
  }
  bool has_expected_encrypt() const {
    return expected_encrypt_.get() != NULL;
  }
  bool has_expected_decrypt() const {
    return expected_decrypt_.get() != NULL;
  }

 private:
  class ExpectedDeriveFromPassword {
   public:
    ExpectedDeriveFromPassword(const string &expected_password,
                                const string &expected_salt,
                                const string &expected_result) :
        expected_password_(expected_password),
        expected_salt_(expected_salt),
        expected_result_(expected_result) {
    }

    const string &expected_password() const {
      return expected_password_;
    }
    const string &expected_salt() const {
      return expected_salt_;
    }
    const string &expected_result() const {
      return expected_result_;
    }
   private:
    string expected_password_;
    string expected_salt_;
    string expected_result_;

    DISALLOW_COPY_AND_ASSIGN(ExpectedDeriveFromPassword);
  };

  class ExpectedCrypt {
   public:
    ExpectedCrypt(const string &expected_data,
                  const string &expected_key,
                  const string &expected_iv,
                  const string &expected_result) :
        expected_data_(expected_data),
        expected_key_(expected_key),
        expected_iv_(expected_iv),
        expected_result_(expected_result) {
    }

    const string &expected_data() const {
      return expected_data_;
    }
    const string &expected_key() const {
      return expected_key_;
    }
    const string &expected_iv() const {
      return expected_iv_;
    }
    const string &expected_result() const {
      return expected_result_;
    }
   private:
    string expected_data_;
    string expected_key_;
    string expected_iv_;
    string expected_result_;

    DISALLOW_COPY_AND_ASSIGN(ExpectedCrypt);
  };

  MockJNIEnv *env_;

  scoped_ptr<ExpectedDeriveFromPassword> expected_derive_from_password_;
  scoped_ptr<ExpectedCrypt> expected_encrypt_;
  scoped_ptr<ExpectedCrypt> expected_decrypt_;

  DISALLOW_COPY_AND_ASSIGN(MockJavaEncryptorImpl);
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

class AndroidJniProxyEncryptorTest : public AndroidJniProxyTest {
 protected:
  virtual void SetUp() {
    AndroidJniProxyTest::SetUp();
    MockJNIEnv *env = jvm_->mutable_env();
    mock_encryptor_ = new MockJavaEncryptorImpl(env);

    // env takes ownership of the mocks.
    env->RegisterMockJavaEncryptor(mock_encryptor_);
    JavaEncryptorProxy::SetJavaVM(jvm_->mutable_jvm());
  }

  virtual void TearDown() {
    JavaEncryptorProxy::SetJavaVM(NULL);
    mock_encryptor_ = NULL;
    AndroidJniProxyTest::TearDown();
  }

  MockJavaEncryptorImpl *mock_encryptor_;
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

TEST_F(AndroidJniProxyEncryptorTest, DeriveFromPassword) {
  mock_encryptor_->ExpectDeriveFromPassword("password", "salt", "result");

  uint8 buf[10];
  size_t buf_size = sizeof(buf);
  ASSERT_TRUE(JavaEncryptorProxy::DeriveFromPassword(
      "password", "salt", buf, &buf_size));
  EXPECT_EQ("result", string(reinterpret_cast<char*>(buf), buf_size));
  EXPECT_FALSE(mock_encryptor_->has_expected_derive_from_password());
}

TEST_F(AndroidJniProxyEncryptorTest, Encrypt) {
  mock_encryptor_->ExpectEncrypt(
      "data",
      "0123456789abcdef0123456789abcdef",
      "0123456789ABCDEF",
      "result");

  char buf[10];
  strcpy(buf, "data");
  size_t buf_size = strlen(buf);
  ASSERT_TRUE(JavaEncryptorProxy::Encrypt(
      reinterpret_cast<const uint8*>("0123456789abcdef0123456789abcdef"),
      reinterpret_cast<const uint8*>("0123456789ABCDEF"),
      sizeof(buf), buf, &buf_size));
  EXPECT_EQ("result", string(buf, buf_size));
  EXPECT_FALSE(mock_encryptor_->has_expected_encrypt());
}

TEST_F(AndroidJniProxyEncryptorTest, Decrypt) {
  mock_encryptor_->ExpectDecrypt(
      "data",
      "0123456789abcdef0123456789abcdef",
      "0123456789ABCDEF",
      "result");

  char buf[10];
  strcpy(buf, "data");
  size_t buf_size = strlen(buf);
  ASSERT_TRUE(JavaEncryptorProxy::Decrypt(
      reinterpret_cast<const uint8*>("0123456789abcdef0123456789abcdef"),
      reinterpret_cast<const uint8*>("0123456789ABCDEF"),
      sizeof(buf), buf, &buf_size));
  EXPECT_EQ("result", string(buf, buf_size));
  EXPECT_FALSE(mock_encryptor_->has_expected_decrypt());
}

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
