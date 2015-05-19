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

#ifndef IME_MOZC_BASE_ANDROID_JNI_MOCK_H_
#define IME_MOZC_BASE_ANDROID_JNI_MOCK_H_

// An utility to mock JNI releated stuff for testing purpose.
#include <jni.h>
#include <map>
#include <utility>

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace jni {

class MockJavaEncryptor {
 public:
  MockJavaEncryptor() {
  }
  virtual ~MockJavaEncryptor() {
  }

  // Following methods are interfaces to emulate the behavior of Encryptor
  // class (in Java). Do nothing by default.
  virtual jbyteArray DeriveFromPassword(jbyteArray password, jbyteArray salt) {
    return NULL;
  }

  virtual jbyteArray Encrypt(jbyteArray data, jbyteArray key, jbyteArray iv) {
    return NULL;
  }

  virtual jbyteArray Decrypt(jbyteArray data, jbyteArray key, jbyteArray iv) {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJavaEncryptor);
};

class MockJavaHttpClient {
 public:
  MockJavaHttpClient() {
  }
  virtual ~MockJavaHttpClient() {
  }

  // Following methods are interfaces to emulate the behavior of HttpClient
  // class (in Java). Do nothing by default.
  virtual jbyteArray Request(
      jbyteArray method, jbyteArray url, jbyteArray postData) {
    return NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockJavaHttpClient);
};

// Mock of jmethodID. jmethodID is a point to _jmethodID which is opaque
// structure. We just need the pointer value of the structure for mocking,
// so we'd hack it based on reinterpret_cast.
struct MockJMethodId {
};

// Mock of JNIEnv. JNIEnv has a pointer of JNINativeInterface, which holds
// function pointers of each API, and some reservedN void pointers.
// To mock the JNIEnv, we abuse "reserved0" to hold a pointer to MockJNIEnv
// instance.
class MockJNIEnv {
 public:
  MockJNIEnv();
  ~MockJNIEnv();

  JNIEnv* mutable_env() {
    return &env_;
  }

  jclass FindClass(const char *class_path);
  jmethodID GetStaticMethodID(
      jclass cls, const char *name, const char *signature);
  jobject CallStaticObjectMethodV(
      jclass cls, jmethodID method, va_list args);

  // Utilities to manage jbyteArray instances.
  jbyteArray NewByteArray(jsize size);
  jsize GetArrayLength(jarray array);
  jbyte *GetByteArrayElements(jbyteArray array, jboolean *is_copy);

  // Register mocked encryptor. This method takes the ownership of
  // mock_encryptor instance.
  void RegisterMockJavaEncryptor(MockJavaEncryptor *mock_encryptor) {
    mock_encryptor_.reset(mock_encryptor);
  }

  // Register mocked http client. This method takes the ownership of
  // mock_http_client instance.
  void RegisterMockJavaHttpClient(MockJavaHttpClient *mock_http_client) {
    mock_http_client_.reset(mock_http_client);
  }

  // Utilities to convert between jbyteArray and string.
  string JByteArrayToString(jbyteArray array);
  jbyteArray StringToJByteArray(const string &str);

 private:
  JNIEnv env_;
  map<jbyteArray, pair<jsize, jbyte*> > byte_array_map_;

  // Encryptor's mock injecting point.
  scoped_ptr<MockJavaEncryptor> mock_encryptor_;
  _jclass mock_encryptor_class_;
  MockJMethodId mock_derive_from_password_;
  MockJMethodId mock_encrypt_;
  MockJMethodId mock_decrypt_;

  // Http client's mock injecting point.
  scoped_ptr<MockJavaHttpClient> mock_http_client_;
  _jclass mock_http_client_class_;
  MockJMethodId mock_request_;

  void SetUpJNIEnv();
  void TearDownJNIEnv();
  void ClearArrayMap();

  static jclass FindClassProxy(JNIEnv *env, const char *class_path);
  static jmethodID GetStaticMethodIDProxy(
      JNIEnv *env, jclass cls, const char *name, const char *signature);
  static jint PushLocalFrameProxy(JNIEnv *, jint);
  static jobject PopLocalFrameProxy(JNIEnv *, jobject);
  static jobject NewGlobalRefProxy(JNIEnv *, jobject obj);
  static void DeleteGlobalRefProxy(JNIEnv *, jobject);
  static jobject CallStaticObjectMethodVProxy(
      JNIEnv *env, jclass cls, jmethodID method, va_list args);
  static jthrowable ExceptionOccurredProxy(JNIEnv *);
  static jbyteArray NewByteArrayProxy(JNIEnv *env, jsize size);
  static jsize GetArrayLengthProxy(JNIEnv *env, jarray array);
  static jbyte* GetByteArrayElementsProxy(
      JNIEnv *env, jbyteArray array, jboolean *is_copy);

  DISALLOW_COPY_AND_ASSIGN(MockJNIEnv);
};

// Mock of JavaVM, similar to MockJNIEnv.
class MockJavaVM {
 public:
  MockJavaVM();
  ~MockJavaVM();

  JavaVM *mutable_jvm() {
    return &jvm_;
  }
  MockJNIEnv *mutable_env() {
    return &env_;
  }

  jint GetEnv(void **env, jint version);

 private:
  JavaVM jvm_;
  MockJNIEnv env_;

  void SetUpJavaVM();
  void TearDownJavaVM();

  static jint GetEnvProxy(JavaVM *jvm, void **env, jint version);

  DISALLOW_COPY_AND_ASSIGN(MockJavaVM);
};

}  // namespace jni
}  // namespace mozc

#endif  // IME_MOZC_BASE_ANDROID_JNI_MOCK_H_
