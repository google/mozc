// Copyright 2010-2018, Google Inc.
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
#include "engine/minimal_engine.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats_uploader.h"

#if !defined(MOZC_USE_CUSTOM_DATA_MANAGER)
// Use plain DataManager, which needs to be manually initialized.
using TargetDataManager = ::mozc::DataManager;
constexpr bool kEmbeddedData = false;


#else
// Use OssDataManager, which is embedding data by default.
#include "data_manager/oss/oss_data_manager.h"
using TargetDataManager = ::mozc::oss::OssDataManager;
constexpr bool kEmbeddedData = true;

#endif

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

// The global instance of Mozc system to be initialized in onPostLoad().
std::unique_ptr<SessionHandlerInterface> g_session_handler;

// Concrete implementation for MozcJni.evalCommand
jbyteArray JNICALL evalCommand(JNIEnv *env,
                               jclass clazz,
                               jbyteArray in_bytes_array) {
  jbyte *in_bytes = env->GetByteArrayElements(in_bytes_array, nullptr);
  const jsize in_size = env->GetArrayLength(in_bytes_array);
  mozc::commands::Command command;
  command.ParseFromArray(in_bytes, in_size);
  if (g_session_handler) {
    g_session_handler->EvalCommand(&command);
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

string JstringToCcString(JNIEnv *env, jstring j_string) {
  const char *cstr = env->GetStringUTFChars(j_string, nullptr);
  const string cc_string(cstr);
  env->ReleaseStringUTFChars(j_string, cstr);
  return cc_string;
}

std::unique_ptr<DataManager> CreateDataManager(JNIEnv *env,
                                               jstring j_data_file_path) {
  if (kEmbeddedData) {
    // If the data manager uses embedded data like OSS version,
    // j_data_file_path is validly nullptr.
    std::unique_ptr<DataManager> data_manager(new TargetDataManager());
    return data_manager;
  }

  if (j_data_file_path == nullptr) {
    return nullptr;
  }
  const string &data_file_path = JstringToCcString(env, j_data_file_path);
  std::unique_ptr<DataManager> data_manager(new TargetDataManager());
  const auto status = data_manager->InitFromFile(data_file_path);
  if (status != DataManager::Status::OK) {
    LOG(INFO) << "Failed to load data file from " << data_file_path << "("
              << DataManager::StatusCodeToString(status) << "). "
              << "Fall back to the embedded data.";
    data_manager.reset();
  }
  return data_manager;
}

std::unique_ptr<SessionHandlerInterface> CreateSessionHandler(
    JNIEnv *env, jstring data_file_path) {
  if (env == nullptr) {
    LOG(DFATAL) << "JNIEnv is null";
    return nullptr;
  }
  std::unique_ptr<DataManager> data_manager =
      CreateDataManager(env, data_file_path);
  std::unique_ptr<EngineInterface> engine;
  if (data_manager) {
    engine = Engine::CreateMobileEngine(std::move(data_manager));
  } else {
    engine.reset(new MinimalEngine());
  }
  std::unique_ptr<SessionHandlerInterface> result(new SessionHandler(
      std::move(engine), std::unique_ptr<EngineBuilder>(new EngineBuilder())));
  result->AddObserver(Singleton<session::SessionUsageObserver>::get());
  return result;
}

// Does post-load tasks.
// Returns true if the task succeeded
// or SessionHandler has already been initializaed.
jboolean JNICALL onPostLoad(JNIEnv *env,
                        jclass clazz,
                        jstring user_profile_directory_path,
                        jstring data_file_path) {
  if (g_session_handler) {
    return true;
  }

  // First of all, set the user profile directory.
  const string &original_dir = SystemUtil::GetUserProfileDirectory();
  SystemUtil::SetUserProfileDirectory(
      JstringToCcString(env, user_profile_directory_path));

  // Initializes Java native callback proxy.
  JavaHttpClientProxy::SetJavaVM(GetJavaVm(env));

  // Initializes the global Mozc system.
  g_session_handler = CreateSessionHandler(env, data_file_path);
  if (!g_session_handler) {
    JavaHttpClientProxy::SetJavaVM(nullptr);
    SystemUtil::SetUserProfileDirectory(original_dir);
    LOG(DFATAL) << "Failed to create Mozc session handler";
    return false;
  }

  // Starts usage stats timer.
  Scheduler::AddJob(GetJobSetting());
  return true;
}

jstring JNICALL getVersion(JNIEnv *env) {
  return env->NewStringUTF(Version::GetMozcVersion().c_str());
}

jstring JNICALL getDataVersion(JNIEnv *env) {
  string version = "";
  if (g_session_handler) {
    g_session_handler->GetDataVersion().CopyToString(&version);
  }
  return env->NewStringUTF(version.c_str());
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
       "(Ljava/lang/String;Ljava/lang/String;)Z",
       reinterpret_cast<void*>(&mozc::jni::onPostLoad)},
      {"getVersion",
       "()Ljava/lang/String;",
       reinterpret_cast<void*>(&mozc::jni::getVersion)},
      {"getDataVersion",
       "()Ljava/lang/String;",
       reinterpret_cast<void*>(&mozc::jni::getDataVersion)},
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
  mozc::jni::g_session_handler.reset();
  mozc::jni::JavaHttpClientProxy::SetJavaVM(nullptr);
}

}  // extern "C"

#endif  // OS_ANDROID
