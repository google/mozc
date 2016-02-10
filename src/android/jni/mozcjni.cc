// Copyright 2010-2016, Google Inc.
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

#ifdef OS_ANDROID

#include <jni.h>

#include <memory>

#include "base/android_jni_proxy.h"
#include "base/android_util.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scheduler.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/version.h"
#include "engine/engine_factory.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats_uploader.h"

#include "data_manager/oss/oss_data_manager.h"
typedef mozc::oss::OssDataManager DataManagerType;

namespace mozc {
namespace jni {
namespace {
jobject g_mozc_data_buffer;

// Returns a job setting for usage stats job.
const Scheduler::JobSetting GetJobSetting() {
  return Scheduler::JobSetting(
      "UsageStatsTimer",
      usage_stats::UsageStatsUploader::kDefaultScheduleInterval,
      usage_stats::UsageStatsUploader::kDefaultScheduleMaxInterval,
      usage_stats::UsageStatsUploader::kDefaultSchedulerDelay,
      usage_stats::UsageStatsUploader::kDefaultSchedulerRandomDelay,
      &usage_stats::UsageStatsUploader::Send,
      NULL);
}

// Adapter class to make a SessionHandlerInterface (held by this class)
// singleton.
// Must be accessed via mozc::Singleton<SessionHandlerSingletonAdapter>.
class SessionHandlerSingletonAdapter {
 public:
  SessionHandlerSingletonAdapter()
      : engine_(mozc::EngineFactory::Create()),
        session_handler_(new mozc::SessionHandler(engine_.get())) {}
  ~SessionHandlerSingletonAdapter() {}

  SessionHandlerInterface *getHandler() {
    return session_handler_.get();
  }

 private:
  // Must be defined earlier than session_handler_, which depends on this.
  std::unique_ptr<EngineInterface> engine_;
  std::unique_ptr<SessionHandlerInterface> session_handler_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandlerSingletonAdapter);
};

void Initialize(JavaVM *vm, const char *user_profile_directory,
                void *mozc_data_address, int mozc_data_size) {
  // First of all, set the user profile directory.
  SystemUtil::SetUserProfileDirectory(user_profile_directory);

  // Initializes Java native callback proxy.
  JavaHttpClientProxy::SetJavaVM(vm);

  // Initializes mozc data.
  DataManagerType::SetMozcDataSet(mozc_data_address, mozc_data_size);

  mozc::Singleton<SessionHandlerSingletonAdapter>::get()->getHandler()
      ->AddObserver(Singleton<session::SessionUsageObserver>::get());

  // Starts usage stats timer.
  mozc::Scheduler::AddJob(GetJobSetting());
}

// Concrete implementation for MozcJni.evalCommand
jbyteArray JNICALL evalCommand(JNIEnv *env,
                               jclass clazz,
                               jbyteArray in_bytes_array) {
  jbyte *in_bytes = env->GetByteArrayElements(in_bytes_array, nullptr);
  const jsize in_size = env->GetArrayLength(in_bytes_array);
  mozc::commands::Command command;
  command.ParseFromArray(in_bytes, in_size);
  mozc::Singleton<SessionHandlerSingletonAdapter>::get()->getHandler()
      ->EvalCommand(&command);

  // Use JNI_ABORT because in_bytes is read only.
  env->ReleaseByteArrayElements(in_bytes_array, in_bytes, JNI_ABORT);

  const int out_size = command.ByteSize();
  jbyteArray out_bytes_array = env->NewByteArray(out_size);
  jbyte *out_bytes = env->GetByteArrayElements(out_bytes_array, nullptr);
  command.SerializeToArray(out_bytes, out_size);

  // Use 0 to copy out_bytes to out_bytes_array.
  env->ReleaseByteArrayElements(out_bytes_array, out_bytes, 0);

  return out_bytes_array;
}

void JNICALL onPostLoad(JNIEnv *env,
                        jclass clazz,
                        jstring user_profile_directory_path,
                        jobject mozc_data_buffer) {
  // Keep the global references of the buffer.
  g_mozc_data_buffer = env->NewGlobalRef(mozc_data_buffer);

  const char *utf8_user_profile_directory_path =
      env->GetStringUTFChars(user_profile_directory_path, nullptr);

  JavaVM *vm = NULL;
  env->GetJavaVM(&vm);

  Initialize(vm, utf8_user_profile_directory_path,
             env->GetDirectBufferAddress(mozc_data_buffer),
             env->GetDirectBufferCapacity(mozc_data_buffer));
  env->ReleaseStringUTFChars(user_profile_directory_path,
                             utf8_user_profile_directory_path);
}

jstring JNICALL getVersion(JNIEnv *env) {
  return env->NewStringUTF(Version::GetMozcVersion().c_str());
}

void JNICALL suppressSendingStats(JNIEnv *env,
                                  jobject clazz,
                                  jboolean suppress) {
  const Scheduler::JobSetting &jobSetting = GetJobSetting();
  const string &name = jobSetting.name();
  const bool hasJob = mozc::Scheduler::HasJob(name);
  if (suppress && hasJob) {
    mozc::Scheduler::RemoveJob(name);
  } else if (!suppress && !hasJob) {
    mozc::Scheduler::AddJob(jobSetting);
  }
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
       "(Ljava/lang/String;Ljava/nio/Buffer;)V",
       reinterpret_cast<void*>(&mozc::jni::onPostLoad)},
      {"getVersion",
       "()Ljava/lang/String;",
       reinterpret_cast<void*>(&mozc::jni::getVersion)},
      {"suppressSendingStats",
       "(Z)V",
       reinterpret_cast<void*>(&mozc::jni::suppressSendingStats)},
  };
  jclass clazz = env->FindClass(
      "org/mozc/android/inputmethod/japanese/session/MozcJNI");
  if (env->RegisterNatives(clazz, methods, arraysize(methods))) {
    // Fatal error. No way to recover.
     return JNI_EVERSION;
  }

  mozc::Logging::InitLogStream("");  // Andorid doesn't stream log to a file.
  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
  mozc::jni::JavaHttpClientProxy::SetJavaVM(NULL);

  // Delete global references.
  JNIEnv *env = mozc::AndroidUtil::GetEnv(vm);
  if (env) {
    env->DeleteGlobalRef(mozc::jni::g_mozc_data_buffer);
  }
}
}  // extern "C"

#endif  // OS_ANDROID
