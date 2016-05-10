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
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/engine_builder.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats_uploader.h"

namespace mozc {
namespace jni {
namespace {

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

// Gets the Java VM from JNIEnv.
JavaVM *GetJavaVm(JNIEnv *env) {
  DCHECK(env);
  JavaVM *vm = nullptr;
  env->GetJavaVM(&vm);
  return vm;
}

// Gets the Mozc data memory range from jobject.
StringPiece GetMozcData(JNIEnv *env, jobject mozc_data_buffer) {
  DCHECK(env);
  const void *addr = env->GetDirectBufferAddress(mozc_data_buffer);
  const size_t size = env->GetDirectBufferCapacity(mozc_data_buffer);
  return StringPiece(static_cast<const char *>(addr), size);
}

// Manages the global reference to Mozc data in Java and the instance of
// SessionHandlerInterface.
class SessionHandlerManager {
 public:
  static SessionHandlerManager *Create(JNIEnv *env, jobject mozc_data_buffer) {
    if (env == nullptr) {
      LOG(DFATAL) << "JNIEnv is null";
      return nullptr;
    }
    std::unique_ptr<DataManager> data_manager(new DataManager());
    const DataManager::Status status =
        data_manager->InitFromArray(GetMozcData(env, mozc_data_buffer));
    if (status != DataManager::Status::OK) {
      LOG(ERROR) << "Error in the data passed through JNI: " << status;
      return nullptr;
    }
    auto manager = new SessionHandlerManager(std::move(data_manager));
    if (!manager) {
      return nullptr;
    }
    manager->mozc_data_buffer_ = env->NewGlobalRef(mozc_data_buffer);
    manager->getHandler()->AddObserver(
        Singleton<session::SessionUsageObserver>::get());
    return manager;
  }

  static void Delete(JNIEnv *env, SessionHandlerManager *manager) {
    if (manager == nullptr) {
      return;
    }
    // The case where env == nullptr is unexpected and usually never happen.
    if (env == nullptr) {
      LOG(DFATAL) << "JNIEnv is null";
      return;
    }
    env->DeleteGlobalRef(manager->mozc_data_buffer_);
    delete manager;
  }

  SessionHandlerInterface *getHandler() {
    return session_handler_.get();
  }

 private:
  explicit SessionHandlerManager(std::unique_ptr<DataManager> data_manager)
      : session_handler_(new SessionHandler(
            Engine::CreateMobileEngine(std::move(data_manager)),
            std::unique_ptr<EngineBuilder>(new EngineBuilder()))) {}

  ~SessionHandlerManager() = default;

  std::unique_ptr<SessionHandlerInterface> session_handler_;
  jobject mozc_data_buffer_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandlerManager);
};

// The global instance of Mozc system to be initialized in onPostLoad().
SessionHandlerManager *g_manager = nullptr;

void InitializeUserProfileDirectory(JNIEnv *env, jstring dir_path) {
  DCHECK(env);
  const char *path = env->GetStringUTFChars(dir_path, nullptr);
  SystemUtil::SetUserProfileDirectory(path);
  env->ReleaseStringUTFChars(dir_path, path);
}

// Concrete implementation for MozcJni.evalCommand
jbyteArray JNICALL evalCommand(JNIEnv *env,
                               jclass clazz,
                               jbyteArray in_bytes_array) {
  jbyte *in_bytes = env->GetByteArrayElements(in_bytes_array, nullptr);
  const jsize in_size = env->GetArrayLength(in_bytes_array);
  mozc::commands::Command command;
  command.ParseFromArray(in_bytes, in_size);
  if (g_manager != nullptr) {
    g_manager->getHandler()->EvalCommand(&command);
  } else {
    LOG(DFATAL) << "Mozc session handler is not yet initialized";
  }

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
  if (g_manager) {
    return;
  }

  // First of all, set the user profile directory.
  const string &original_dir = SystemUtil::GetUserProfileDirectory();
  InitializeUserProfileDirectory(env, user_profile_directory_path);

  // Initializes Java native callback proxy.
  JavaHttpClientProxy::SetJavaVM(GetJavaVm(env));

  // Initializes the global Mozc system.
  g_manager = SessionHandlerManager::Create(env, mozc_data_buffer);
  if (g_manager == nullptr) {
    JavaHttpClientProxy::SetJavaVM(nullptr);
    SystemUtil::SetUserProfileDirectory(original_dir);
    LOG(DFATAL) << "Failed to create Mozc session handler";
    return;
  }

  // Starts usage stats timer.
  Scheduler::AddJob(GetJobSetting());
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
  mozc::jni::SessionHandlerManager::Delete(mozc::AndroidUtil::GetEnv(vm),
                                           mozc::jni::g_manager);
  mozc::jni::g_manager = nullptr;
  mozc::jni::JavaHttpClientProxy::SetJavaVM(nullptr);
}

}  // extern "C"

#endif  // OS_ANDROID
