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
#include "base/mutex.h"
#include "base/scheduler.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/version.h"
#include "converter/connector_interface.h"
#include "dictionary/dictionary_interface.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
#include "session/session_factory_manager.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats_uploader.h"

#include "data_manager/oss/oss_data_manager.h"

namespace mozc {
namespace jni {
namespace {
jobject g_connection_data_buffer;
jobject g_dictionary_buffer;

class SessionFactoryManager {
 public:
  SessionFactoryManager()
      : engine_(EngineFactory::Create()),
        session_factory_(new session::JapaneseSessionFactory(engine_.get())) {}

  session::JapaneseSessionFactory *mutable_session_factory() {
    return session_factory_.get();
  }

 private:
  scoped_ptr<EngineInterface> engine_;
  scoped_ptr<session::JapaneseSessionFactory> session_factory_;
};

void Initialize(
    JavaVM *vm,
    const char *user_profile_directory,
    void *dictionary_address, int dictionary_size,
    void *connection_data_address, int connection_data_size) {
  // First of all, set the user profile directory.
  SystemUtil::SetUserProfileDirectory(user_profile_directory);

  // Initialize Java native callback proxies.
  JavaEncryptorProxy::SetJavaVM(vm);
#ifdef MOZC_ENABLE_HTTP_CLIENT
  JavaHttpClientProxy::SetJavaVM(vm);
#endif  // MOZC_ENABLE_HTTP_CLIENT

  // Initialize dictionary data.
  mozc::oss::OssDataManager::SetDictionaryData(
      dictionary_address, dictionary_size);
  mozc::oss::OssDataManager::SetConnectionData(
      connection_data_address, connection_data_size);

  // Initialize the session factory.
  session::SessionFactoryManager::SetSessionFactory(
      Singleton<SessionFactoryManager>::get()->mutable_session_factory());
  Singleton<SessionHandler>::get()->
      AddObserver(Singleton<session::SessionUsageObserver>::get());

  // Start usage stats timer.
  using usage_stats::UsageStatsUploader;
  Scheduler::AddJob(Scheduler::JobSetting(
      "UsageStatsTimer",
      UsageStatsUploader::kDefaultScheduleInterval,
      UsageStatsUploader::kDefaultScheduleMaxInterval,
      UsageStatsUploader::kDefaultSchedulerDelay,
      UsageStatsUploader::kDefaultSchedulerRandomDelay,
      &UsageStatsUploader::Send,
      NULL));
}

// Concrete implementation for MozcJni.evalCommand
jbyteArray JNICALL evalCommand(JNIEnv *env,
                               jclass clazz,
                               jbyteArray in_bytes_array) {
  jboolean is_copy = false;
  jbyte *in_bytes = env->GetByteArrayElements(in_bytes_array, &is_copy);
  const jsize in_size = env->GetArrayLength(in_bytes_array);
  mozc::commands::Command command;
  command.ParseFromArray(in_bytes, in_size);
  mozc::Singleton<mozc::SessionHandler>::get()->EvalCommand(&command);

  // Use JNI_ABORT because in_bytes is read only.
  env->ReleaseByteArrayElements(in_bytes_array, in_bytes, JNI_ABORT);

  const int out_size = command.ByteSize();
  jbyteArray out_bytes_array = env->NewByteArray(out_size);
  jbyte *out_bytes = env->GetByteArrayElements(out_bytes_array, &is_copy);
  command.SerializeToArray(out_bytes, out_size);

  // Use 0 to copy out_bytes to out_bytes_array.
  env->ReleaseByteArrayElements(out_bytes_array, out_bytes, 0);

  return out_bytes_array;
}

void JNICALL onPostLoad(JNIEnv *env,
                        jclass clazz,
                        jstring user_profile_directory_path,
                        jobject dictionary_buffer,
                        jobject connection_data_buffer) {
  // Keep the global references of the buffers.
  g_dictionary_buffer = env->NewGlobalRef(dictionary_buffer);
  g_connection_data_buffer = env->NewGlobalRef(connection_data_buffer);

  jboolean is_copy = JNI_FALSE;
  const char *utf8_user_profile_directory_path =
      env->GetStringUTFChars(user_profile_directory_path, &is_copy);

  JavaVM *vm = NULL;
  env->GetJavaVM(&vm);

  Initialize(
      vm, utf8_user_profile_directory_path,
      env->GetDirectBufferAddress(dictionary_buffer),
      env->GetDirectBufferCapacity(dictionary_buffer),
      env->GetDirectBufferAddress(connection_data_buffer),
      env->GetDirectBufferCapacity(connection_data_buffer));
  if (is_copy) {
    env->ReleaseStringUTFChars(user_profile_directory_path,
                               utf8_user_profile_directory_path);
  }
}

jstring JNICALL getVersion(JNIEnv *env) {
  return env->NewStringUTF(Version::GetMozcVersion().c_str());
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
      {"evalCommand",
       "([B)[B",
       reinterpret_cast<void*>(&mozc::jni::evalCommand)},
      {"onPostLoad",
       "(Ljava/lang/String;Ljava/nio/Buffer;Ljava/nio/Buffer;)V",
       reinterpret_cast<void*>(&mozc::jni::onPostLoad)},
      {"getVersion",
       "()Ljava/lang/String;",
       reinterpret_cast<void*>(&mozc::jni::getVersion)},
  };
  jclass clazz = env->FindClass(
      "org/mozc/android/inputmethod/japanese/session/MozcJNI");
  if (env->RegisterNatives(clazz, methods, arraysize(methods))) {
    // Fatal error. No way to recover.
     return JNI_EVERSION;
  }

  mozc::Logging::InitLogStream("libmozc.so");
  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
  mozc::jni::JavaEncryptorProxy::SetJavaVM(NULL);
#ifdef MOZC_ENABLE_HTTP_CLIENT
  mozc::jni::JavaHttpClientProxy::SetJavaVM(NULL);
#endif  // MOZC_ENABLE_HTTP_CLIENT

  // Delete global references.
  JNIEnv *env = mozc::AndroidUtil::GetEnv(vm);
  if (env) {
    env->DeleteGlobalRef(mozc::jni::g_connection_data_buffer);
    env->DeleteGlobalRef(mozc::jni::g_dictionary_buffer);
  }
}
}  // extern "C"
