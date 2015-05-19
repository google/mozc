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

// JNI wrapper for SessionHandler.

#include <jni.h>

#include "base/android_jni_proxy.h"
#include "base/android_util.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "net/http_client.h"

namespace mozc {
namespace jni {
namespace {
void Init() {
  Logging::InitLogStream("libjnitestingbackdoor.so");
}

jbyteArray JNICALL httpRequest(JNIEnv *env,
                               jclass clazz,
                               jbyteArray method,
                               jbyteArray url,
                               jbyteArray content) {
  jboolean is_copy = false;

  jbyte *method_bytes = env->GetByteArrayElements(method, &is_copy);
  const string method_string(reinterpret_cast<const char*>(method_bytes),
                             env->GetArrayLength(method));
  env->ReleaseByteArrayElements(method, method_bytes, JNI_ABORT);

  jbyte *url_bytes = env->GetByteArrayElements(url, &is_copy);
  const string url_string(reinterpret_cast<const char*>(url_bytes),
                          env->GetArrayLength(url));
  env->ReleaseByteArrayElements(url, url_bytes, JNI_ABORT);

  bool result = false;
  string output;

  mozc::HTTPClient::Option option;
  option.timeout = 10 * 1000;  // 10 sec.

  LOG(INFO) << "method length: " << env->GetArrayLength(url);
  LOG(INFO) << "method: '" << method_string << "'";
  LOG(INFO) << "url: '" << url_string << "'";
  if (method_string == "GET") {
    LOG(INFO) << "GET";
    result = mozc::HTTPClient::Get(url_string.c_str(), option, &output);
  } else if (method_string == "HEAD") {
    LOG(INFO) << "HEAD";
    result = mozc::HTTPClient::Head(url_string.c_str(), option, &output);
  } else if (method_string == "POST" && content) {
    LOG(INFO) << "POST";
    jbyte *content_bytes = env->GetByteArrayElements(content, &is_copy);
    const string content_string(reinterpret_cast<const char*>(content_bytes),
                                env->GetArrayLength(content));
    env->ReleaseByteArrayElements(content, content_bytes, JNI_ABORT);
    LOG(INFO) << "content: '" << content_string << "'";
    result = mozc::HTTPClient::Post(url_string.c_str(),
                                    content_string, option, &output);
  } else {
    LOG(INFO) << "invalid method";
  }

  if (result) {
    jbyteArray out_bytes_array = env->NewByteArray(output.size());
    jbyte *out_bytes = env->GetByteArrayElements(out_bytes_array, &is_copy);
    memcpy(out_bytes, output.c_str(), output.size());

    // Use 0 to copy out_bytes to out_bytes_array.
    env->ReleaseByteArrayElements(out_bytes_array, out_bytes, 0);

    return out_bytes_array;
  }
  return NULL;
}
}  // namespace
}  // namespace jni
}  // namespace mozc

extern "C" {
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  JNIEnv *env = mozc::AndroidUtil::GetEnv(vm);
  if (!env) {
    // Fatal error. No way to recover.
    return JNI_EVERSION;
  }
  const JNINativeMethod methods[] = {
      {"httpRequest",
       "([B[B[B)[B",
       reinterpret_cast<void*>(&mozc::jni::httpRequest)},
  };
  jclass clazz = env->FindClass(
      "org/mozc/android/inputmethod/japanese/session/JNITestingBackdoor");
  if (env->RegisterNatives(clazz, methods, arraysize(methods))) {
    // Fatal error. No way to recover.
     return JNI_EVERSION;
  }

  static mozc::once_t once = MOZC_ONCE_INIT;
  mozc::CallOnce(&once, &mozc::jni::Init);
  mozc::jni::JavaHttpClientProxy::SetJavaVM(vm);

  return JNI_VERSION_1_6;
}
}  // extern "C"
