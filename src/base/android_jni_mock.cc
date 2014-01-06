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

#include "base/android_jni_mock.h"

#include "base/logging.h"

namespace mozc {
namespace jni {

MockJNIEnv::MockJNIEnv() {
  SetUpJNIEnv();
}

MockJNIEnv::~MockJNIEnv() {
  ClearArrayMap();
  TearDownJNIEnv();
}

// Setting up a JNIEnv instance.
void MockJNIEnv::SetUpJNIEnv() {
  JNINativeInterface *functions = new JNINativeInterface;
  functions->reserved0 = this;
  functions->FindClass = MockJNIEnv::FindClassProxy;
  functions->GetStaticMethodID = MockJNIEnv::GetStaticMethodIDProxy;
  functions->PushLocalFrame = MockJNIEnv::PushLocalFrameProxy;
  functions->PopLocalFrame = MockJNIEnv::PopLocalFrameProxy;
  functions->NewGlobalRef = MockJNIEnv::NewGlobalRefProxy;
  functions->DeleteGlobalRef = MockJNIEnv::DeleteGlobalRefProxy;

  functions->CallStaticObjectMethodV =
      MockJNIEnv::CallStaticObjectMethodVProxy;
  functions->ExceptionOccurred = MockJNIEnv::ExceptionOccurredProxy;

  functions->NewByteArray = MockJNIEnv::NewByteArrayProxy;
  functions->GetArrayLength = MockJNIEnv::GetArrayLengthProxy;
  functions->GetByteArrayRegion = MockJNIEnv::GetByteArrayRegionProxy;
  functions->SetByteArrayRegion = MockJNIEnv::SetByteArrayRegionProxy;

  env_.functions = functions;
}

void MockJNIEnv::TearDownJNIEnv() {
  delete env_.functions;
}

void MockJNIEnv::ClearArrayMap() {
  for (map<jbyteArray, pair<jsize, jbyte*> >::iterator iter =
           byte_array_map_.begin();
       iter != byte_array_map_.end(); ++iter) {
    delete iter->first;
    delete [] iter->second.second;
  }
  byte_array_map_.clear();
}

jclass MockJNIEnv::FindClass(const char *class_path) {
  static const char kEncryptorPath[] =
      "org/mozc/android/inputmethod/japanese/nativecallback/Encryptor";
  if (strcmp(class_path, kEncryptorPath) == 0) {
    return &mock_encryptor_class_;
  }
  static const char kHttpClientPath[] =
      "org/mozc/android/inputmethod/japanese/nativecallback/HttpClient";
  if (strcmp(class_path, kHttpClientPath) == 0) {
    return &mock_http_client_class_;
  }
  return NULL;
}

jmethodID MockJNIEnv::GetStaticMethodID(
    jclass cls, const char *name, const char *signature) {
  if (cls == &mock_encryptor_class_) {
    if (strcmp(name, "deriveFromPassword") == 0 &&
        strcmp(signature, "([B[B)[B") == 0) {
      return reinterpret_cast<jmethodID>(&mock_derive_from_password_);
    }
    if (strcmp(name, "encrypt") == 0 &&
        strcmp(signature, "([B[B[B)[B") == 0) {
      return reinterpret_cast<jmethodID>(&mock_encrypt_);
    }
    if (strcmp(name, "decrypt") == 0 &&
        strcmp(signature, "([B[B[B)[B") == 0) {
      return reinterpret_cast<jmethodID>(&mock_decrypt_);
    }
    return NULL;
  }
  if (cls == &mock_http_client_class_) {
    if (strcmp(name, "request") == 0 &&
        strcmp(signature, "([B[B[B)[B") == 0) {
      return reinterpret_cast<jmethodID>(&mock_request_);
    }
    return NULL;
  }
  return NULL;
}

jobject MockJNIEnv::CallStaticObjectMethodV(
    jclass cls, jmethodID method, va_list args) {
  if (cls == &mock_encryptor_class_) {
    CHECK(mock_encryptor_.get() != NULL)
        << "mock_encryptor_ is not initialized.";
    if (method == reinterpret_cast<jmethodID>(&mock_derive_from_password_)) {
      jbyteArray password = va_arg(args, jbyteArray);
      jbyteArray salt = va_arg(args, jbyteArray);
      return mock_encryptor_->DeriveFromPassword(password, salt);
    }
    if (method == reinterpret_cast<jmethodID>(&mock_encrypt_)) {
      jbyteArray data = va_arg(args, jbyteArray);
      jbyteArray key = va_arg(args, jbyteArray);
      jbyteArray iv = va_arg(args, jbyteArray);
      return mock_encryptor_->Encrypt(data, key, iv);
    }
    if (method == reinterpret_cast<jmethodID>(&mock_decrypt_)) {
      jbyteArray data = va_arg(args, jbyteArray);
      jbyteArray key = va_arg(args, jbyteArray);
      jbyteArray iv = va_arg(args, jbyteArray);
      return mock_encryptor_->Decrypt(data, key, iv);
    }
    LOG(FATAL) << "Unexpected call.";
    return NULL;
  }
  if (cls == &mock_http_client_class_) {
    CHECK(mock_http_client_.get() != NULL)
        << "mock_http_client_ is not initialized.";
    if (method == reinterpret_cast<jmethodID>(&mock_request_)) {
      jbyteArray method = va_arg(args, jbyteArray);
      jbyteArray url = va_arg(args, jbyteArray);
      jbyteArray post_data = va_arg(args, jbyteArray);
      return mock_http_client_->Request(method, url, post_data);
    }
    LOG(FATAL) << "Unexpected call.";
    return NULL;
  }
  LOG(FATAL) << "Unexpected call.";
  return NULL;
}

// Utilities to manage jbyteArray instances.
jbyteArray MockJNIEnv::NewByteArray(jsize size) {
  // Use non-public type _jbyteArray, which is the dereferenced type of
  // jbyteArray. We can use the same technique as MockJMethodId if necessary,
  // but this hack looks simpler.
  jbyteArray result = new _jbyteArray;
  jbyte *buf = new jbyte[size];
  byte_array_map_[result] = make_pair(size, buf);
  return result;
}

jsize MockJNIEnv::GetArrayLength(jarray array) {
  map<jbyteArray, pair<jsize, jbyte*> >::iterator iter =
      byte_array_map_.find(static_cast<jbyteArray>(array));
  if (iter != byte_array_map_.end()) {
    return iter->second.first;
  }
  return 0;
}

jbyte *MockJNIEnv::GetByteArrayElements(jbyteArray array, jboolean *is_copy) {
  map<jbyteArray, pair<jsize, jbyte*> >::iterator iter =
      byte_array_map_.find(array);
  if (iter != byte_array_map_.end()) {
    if (is_copy) {
      *is_copy = JNI_FALSE;
    }
    return iter->second.second;
  }
  return NULL;
}
void MockJNIEnv::GetByteArrayRegion(
      jbyteArray array, jsize start, jsize len, jbyte *buf) {
  const jsize size = GetArrayLength(array);
  CHECK(start <= size);
  CHECK(start + len <= size);
  memcpy(buf, GetByteArrayElements(array, NULL) + start, len);
}

void MockJNIEnv::SetByteArrayRegion(
      jbyteArray array, jsize start, jsize len, const jbyte *buf) {
  const jsize size = GetArrayLength(array);
  CHECK(start <= size);
  CHECK(start + len <= size);
  memcpy(GetByteArrayElements(array, NULL) + start, buf, len);
}

string MockJNIEnv::JByteArrayToString(jbyteArray array) {
  jboolean is_copy;
  jbyte *buffer = GetByteArrayElements(array, &is_copy);
  CHECK(!is_copy);
  return string(reinterpret_cast<char*>(buffer), GetArrayLength(array));
}

jbyteArray MockJNIEnv::StringToJByteArray(const string &str) {
  jbyteArray result = NewByteArray(str.size());
  jboolean is_copy;
  jbyte *buffer = GetByteArrayElements(result, &is_copy);
  CHECK(!is_copy);
  memcpy(buffer, str.data(), str.size());
  return result;
}

// static proxy methods.
jclass MockJNIEnv::FindClassProxy(JNIEnv *env, const char *class_path) {
  return static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->FindClass(class_path);
}

jmethodID MockJNIEnv::GetStaticMethodIDProxy(
    JNIEnv *env, jclass cls, const char *name, const char *signature) {
  return static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->GetStaticMethodID(cls, name, signature);
}

jint MockJNIEnv::PushLocalFrameProxy(JNIEnv *, jint) {
  return 0;
}

jobject MockJNIEnv::PopLocalFrameProxy(JNIEnv *, jobject) {
  return NULL;
}

jobject MockJNIEnv::NewGlobalRefProxy(JNIEnv *, jobject obj) {
  return obj;
}

void MockJNIEnv::DeleteGlobalRefProxy(JNIEnv *, jobject) {
}

jobject MockJNIEnv::CallStaticObjectMethodVProxy(
    JNIEnv *env, jclass cls, jmethodID method, va_list args) {
  return static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->CallStaticObjectMethodV(cls, method, args);
}

jthrowable MockJNIEnv::ExceptionOccurredProxy(JNIEnv *) {
  return NULL;
}

jbyteArray MockJNIEnv::NewByteArrayProxy(JNIEnv *env, jsize size) {
  return static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->NewByteArray(size);
}

jsize MockJNIEnv::GetArrayLengthProxy(JNIEnv *env, jarray array) {
  return static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->GetArrayLength(array);
}

void MockJNIEnv::GetByteArrayRegionProxy(
      JNIEnv *env, jbyteArray array, jsize start, jsize len, jbyte *buf) {
  static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->GetByteArrayRegion(array, start, len, buf);
}

void MockJNIEnv::SetByteArrayRegionProxy(
      JNIEnv *env, jbyteArray array, jsize start, jsize len, const jbyte *buf) {
  static_cast<MockJNIEnv*>(env->functions->reserved0)
      ->SetByteArrayRegion(array, start, len, buf);
}

MockJavaVM::MockJavaVM() {
  SetUpJavaVM();
}

MockJavaVM::~MockJavaVM() {
  TearDownJavaVM();
}

jint MockJavaVM::GetEnv(void **env, jint version) {
  *env = env_.mutable_env();
  return JNI_OK;
}

jint MockJavaVM::GetEnvProxy(JavaVM *jvm, void **env, jint version) {
  return static_cast<MockJavaVM*>(jvm->functions->reserved0)
      ->GetEnv(env, version);
}

void MockJavaVM::SetUpJavaVM() {
  JNIInvokeInterface *functions = new JNIInvokeInterface;
  functions->reserved0 = this;
  functions->GetEnv = MockJavaVM::GetEnvProxy;

  jvm_.functions = functions;
}

void MockJavaVM::TearDownJavaVM() {
  delete jvm_.functions;
}

}  // jni
}  // mozc
