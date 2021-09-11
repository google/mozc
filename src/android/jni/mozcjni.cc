// Copyright 2010-2021, Google Inc.
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

#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/engine_builder.h"
#include "engine/minimal_engine.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace jni {
namespace {

// The global instance of Mozc system to be initialized in onPostLoad().
std::unique_ptr<SessionHandlerInterface> g_session_handler;

// Concrete implementation for MozcJni.evalCommand
jbyteArray JNICALL evalCommand(JNIEnv *env, jclass clazz,
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

  const int out_size = command.ByteSizeLong();
  jbyteArray out_bytes_array = env->NewByteArray(out_size);
  jbyte *out_bytes = env->GetByteArrayElements(out_bytes_array, nullptr);
  command.SerializeToArray(out_bytes, out_size);

  // Use 0 to copy out_bytes to out_bytes_array.
  env->ReleaseByteArrayElements(out_bytes_array, out_bytes, 0);

  return out_bytes_array;
}

std::string JstringToCcString(JNIEnv *env, jstring j_string) {
  const char *cstr = env->GetStringUTFChars(j_string, nullptr);
  const std::string cc_string(cstr);
  env->ReleaseStringUTFChars(j_string, cstr);
  return cc_string;
}

absl::StatusOr<std::unique_ptr<DataManager>> CreateDataManager(
    const std::string &data_file_path) {
  auto data_manager = absl::make_unique<DataManager>();
  const auto status = data_manager->InitFromFile(data_file_path);
  if (status != DataManager::Status::OK) {
    return absl::FailedPreconditionError(
        DataManager::StatusCodeToString(status));
  }
  return data_manager;
}

std::unique_ptr<EngineInterface> CreateMobileEngine(
    const std::string &data_file_path) {
  auto data_manager = CreateDataManager(data_file_path);
  if (!data_manager.ok()) {
    LOG(ERROR) << "Failed to create a data manager from " << data_file_path
               << ": " << data_manager.status()
               << ": Fallback to minimal engine";
    return absl::make_unique<MinimalEngine>();
  }
  const absl::string_view data_version = (*data_manager)->GetDataVersion();
  auto engine = Engine::CreateMobileEngine(*std::move(data_manager));
  if (!engine.ok()) {
    LOG(ERROR) << "Failed to create a mobile engine: file " << data_file_path
               << ", data version: " << data_version << ": " << engine.status()
               << ": Fallback to minimal engine";
    return absl::make_unique<MinimalEngine>();
  }
  LOG(INFO) << "Successfully created a mobile engine from " << data_file_path
            << ", data version=" << data_version;
  return *std::move(engine);
}

std::unique_ptr<SessionHandlerInterface> CreateSessionHandler(
    JNIEnv *env, jstring j_data_file_path) {
  if (env == nullptr) {
    LOG(DFATAL) << "JNIEnv is null";
    return nullptr;
  }
  std::unique_ptr<EngineInterface> engine;
  if (j_data_file_path == nullptr) {
    LOG(ERROR) << "j_data_file_path is null.  Fallback to minimal engine.";
    engine = absl::make_unique<MinimalEngine>();
  } else {
    const std::string &data_file_path =
        JstringToCcString(env, j_data_file_path);
    engine = CreateMobileEngine(data_file_path);
  }
  DCHECK(engine);
  auto result = absl::make_unique<SessionHandler>(
      std::move(engine), absl::make_unique<EngineBuilder>());
  result->AddObserver(Singleton<session::SessionUsageObserver>::get());
  return result;
}

// Does post-load tasks.
// Returns true if the task succeeded
// or SessionHandler has already been initializaed.
jboolean JNICALL onPostLoad(JNIEnv *env, jclass clazz,
                            jstring user_profile_directory_path,
                            jstring data_file_path) {
  if (g_session_handler) {
    return true;
  }

  // First of all, set the user profile directory.
  const std::string &original_dir = SystemUtil::GetUserProfileDirectory();
  SystemUtil::SetUserProfileDirectory(
      JstringToCcString(env, user_profile_directory_path));

  // Initializes the global Mozc system.
  g_session_handler = CreateSessionHandler(env, data_file_path);
  if (!g_session_handler) {
    SystemUtil::SetUserProfileDirectory(original_dir);
    LOG(DFATAL) << "Failed to create Mozc session handler";
    return false;
  }

  return true;
}

jstring JNICALL getDataVersion(JNIEnv *env) {
  std::string version = "";
  if (g_session_handler) {
    const absl::string_view version_sp = g_session_handler->GetDataVersion();
    version.assign(version_sp.begin(), version_sp.end());
  }
  return env->NewStringUTF(version.c_str());
}

}  // namespace
}  // namespace jni
}  // namespace mozc

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_google_android_inputmethod_japanese_session_MozcJNI_initialize(
    JNIEnv *env, jclass clazz) {
  if (!env) {
    // Fatal error. No way to recover.
    return false;
  }
  const JNINativeMethod methods[] = {
      {"evalCommand", "([B)[B",
       reinterpret_cast<void *>(&mozc::jni::evalCommand)},
      {"onPostLoad", "(Ljava/lang/String;Ljava/lang/String;)Z",
       reinterpret_cast<void *>(&mozc::jni::onPostLoad)},
      {"getDataVersion", "()Ljava/lang/String;",
       reinterpret_cast<void *>(&mozc::jni::getDataVersion)},
  };
  if (env->RegisterNatives(clazz, methods, std::size(methods))) {
    // Fatal error. No way to recover.
    return false;
  }

  mozc::Logging::InitLogStream("");  // Android doesn't stream log to a file.
  return true;
}

}  // extern "C"

#endif  // OS_ANDROID
