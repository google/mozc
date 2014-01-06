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

// TODO(horo): write tests.

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <queue>
#include <string>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/pepper_file_util.h"
#include "base/scheduler.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/version.h"
#include "chrome/nacl/dictionary_downloader.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "data_manager/packed/packed_data_manager.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "engine/engine_factory.h"
#include "net/http_client.h"
#include "net/http_client_pepper.h"
#include "net/jsoncpp.h"
#include "net/json_util.h"
#include "session/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_uploader.h"

using mozc::net::JsonUtil;

namespace mozc {
namespace {

// TODO(horo): Need to confirm that this 1024 is OK.
const uint32 kFileIoFileSystemExpectedSize = 1024;

// Wrapper class of pthread_cond.
class Condition {
 public:
  Condition() {
    pthread_cond_init(&cond_, NULL);
  }
  ~Condition() {
    pthread_cond_destroy(&cond_);
  }
  int signal() {
    return pthread_cond_signal(&cond_);
  }
  int broadcast() {
    return pthread_cond_broadcast(&cond_);
  }
  void wait(Mutex *mutex) {
    pthread_cond_wait(&cond_, mutex->raw_mutex());
  }

 private:
  pthread_cond_t cond_;
  DISALLOW_COPY_AND_ASSIGN(Condition);
};

// Simple blocking queue implementation.
template<typename T>
class BlockingQueue {
 public:
  BlockingQueue()
    : blocked_count_(0),
      is_stopped_(false) {
  }

  ~BlockingQueue() {
    stop();
  }

  void stop() {
    scoped_lock l(&mutex_);
    is_stopped_ = true;
    condition_.broadcast();
    while (blocked_count_) {
      condition_.wait(&mutex_);
    }
  }

  void put(T element) {
    scoped_lock l(&mutex_);
    queue_.push(element);
    condition_.signal();
  }

  T take(bool *stopped) {
    if (stopped) {
      *stopped = false;
    }
    scoped_lock l(&mutex_);
    ++blocked_count_;
    while (queue_.empty() && !is_stopped_) {
      condition_.wait(&mutex_);
    }
    --blocked_count_;

    if (is_stopped_) {
      condition_.broadcast();
      if (stopped) {
        *stopped = true;
      }
    }
    const T front_element = queue_.front();
    queue_.pop();
    return front_element;
  }

 private:
  Mutex mutex_;
  Condition condition_;
  int blocked_count_;
  bool is_stopped_;
  queue<T> queue_;

  DISALLOW_COPY_AND_ASSIGN(BlockingQueue);
};

}  // namespace

namespace session {

class MozcSessionHandlerThread : public Thread {
 public:
  MozcSessionHandlerThread(
      pp::Instance *instance,
      BlockingQueue<Json::Value*> *queue)
    : instance_(instance), message_queue_(queue), factory_(this) {
  }

  virtual ~MozcSessionHandlerThread() {
  }

  virtual void Run() {
    Util::SetRandomSeed(static_cast<uint32>(Util::GetTime()));
    RegisterPepperInstanceForHTTPClient(instance_);
    PepperFileUtil::Initialize(instance_, kFileIoFileSystemExpectedSize);
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    if (!LoadBigDictionary(&big_dictionary_version_)) {
      LOG(ERROR) << "LoadBigDictionary error";
      StartDownloadDictionary();
      LoadDictionary();
    } else if (big_dictionary_version_ !=
                   Version::GetMozcNaclDictionaryVersion()) {
      LOG(ERROR) << "LoadBigDictionary version miss match "
                 << big_dictionary_version_ << " :"
                 << Version::GetMozcNaclDictionaryVersion();
      StartDownloadDictionary();
    }
#else  // GOOGLE_JAPANESE_INPUT_BUILD
    LoadDictionary();
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    user_pos_.reset(
        new UserPOS(
            packed::PackedDataManager::GetUserPosManager()->GetUserPOSData()));

    engine_.reset(mozc::EngineFactory::Create());
    handler_.reset(new SessionHandler(engine_.get()));

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    usage_observer_.reset(new SessionUsageObserver());
    handler_->AddObserver(usage_observer_.get());

    // start usage stats timer
    // send usage stats within 5 min later
    // attempt to send every 5 min -- 2 hours.
    Scheduler::AddJob(Scheduler::JobSetting(
        "UsageStatsTimer",
        usage_stats::UsageStatsUploader::kDefaultScheduleInterval,
        usage_stats::UsageStatsUploader::kDefaultScheduleMaxInterval,
        usage_stats::UsageStatsUploader::kDefaultSchedulerDelay,
        usage_stats::UsageStatsUploader::kDefaultSchedulerRandomDelay,
        &MozcSessionHandlerThread::SendUsageStats,
        this));
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

    // Gets the current config.
    config::Config config;
    config::ConfigHandler::GetStoredConfig(&config);
    // Sends "InitializeDone" message to JavaScript side code.
    Json::Value message(Json::objectValue);
    message["event"] = Json::objectValue;
    message["event"]["type"] = "InitializeDone";
    JsonUtil::ProtobufMessageToJsonValue(config, &message["event"]["config"]);
    message["event"]["version"] = Version::GetMozcVersion();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    message["event"]["big_dictionary_version"] = big_dictionary_version_;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

    pp::Module::Get()->core()->CallOnMainThread(
        0,
        factory_.NewCallback(
            &MozcSessionHandlerThread::PostMessage,
            Json::FastWriter().write(message)));

    while (true) {
      bool stopped = false;
      scoped_ptr<Json::Value> message;
      message.reset(message_queue_->take(&stopped));
      if (stopped) {
        LOG(ERROR) << " message_queue_ stopped";
        return;
      }
      if (!message->isMember("id") ||
          (!message->isMember("cmd") && !message->isMember("event"))) {
        LOG(ERROR) << "request error";
        continue;
      }
      Json::Value response(Json::objectValue);
      response["id"] = (*message)["id"];
      if (message->isMember("cmd")) {
        commands::Command command;
        JsonUtil::JsonValueToProtobufMessage((*message)["cmd"], &command);
        handler_->EvalCommand(&command);
        JsonUtil::ProtobufMessageToJsonValue(command, &response["cmd"]);
      }
      if (message->isMember("event") && (*message)["event"].isMember("type")) {
        response["event"] = Json::objectValue;
        const string event_type = (*message)["event"]["type"].asString();
        response["event"]["type"] = event_type;
        if (event_type == "SyncToFile") {
          response["event"]["result"] = PepperFileUtil::SyncMmapToFile();
        } else if (event_type == "GetVersionInfo") {
          response["event"]["version"] = Version::GetMozcVersion();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
          response["event"]["big_dictionary_version"] = big_dictionary_version_;
          response["event"]["big_dictionary_state"] = GetBigDictionaryState();
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
        } else if (event_type == "GetPosList") {
          GetPosList(&response);
        } else if (event_type == "IsValidReading") {
          IsValidReading((*message)["event"], &response);
        } else {
          response["event"]["error"] = "Unsupported event";
        }
      }

      pp::Module::Get()->core()->CallOnMainThread(
          0,
          factory_.NewCallback(
              &MozcSessionHandlerThread::PostMessage,
              Json::FastWriter().write(response)));
    }
  }

  void PostMessage(int32_t result, const string &message) {
    instance_->PostMessage(message);
  }

 private:
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  // Loads the big dictionary
  // Returns true and sets the dictionary version if successful.
  bool LoadBigDictionary(string *version) {
    string buffer;
    // The big dictionary data is in the user's HTML5 file system.
    if (!PepperFileUtil::ReadBinaryFile("/zipped_data_google", &buffer)) {
      LOG(ERROR) << "PepperFileUtil::ReadBinaryFile error";
      return false;
    }
    scoped_ptr<mozc::packed::PackedDataManager>
        data_manager(new mozc::packed::PackedDataManager());
    if (!data_manager->InitWithZippedData(buffer)) {
      LOG(ERROR) << "InitWithZippedData error";
      return false;
    }
    *version = data_manager->GetDictionaryVersion();
    mozc::packed::RegisterPackedDataManager(data_manager.release());
    return true;
  }

  // Starts downloading the big dictionary.
  void StartDownloadDictionary() {
    downloader_.reset(new chrome::nacl::DictionaryDownloader(
        Version::GetMozcNaclDictionaryUrl(),
        "/zipped_data_google"));
    downloader_->SetOption(10 * 60 * 1000,  // 10 minutes start delay
                           20 * 60 * 1000,  // + [0-20] minutes random delay
                           30 * 60 * 1000,  // retry_interval 30 min
                           4,  // retry interval [30, 60, 120, 240, 240, 240...]
                           10);  // 10 retries
    downloader_->StartDownload();
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  // Loads the dictionary.
  void LoadDictionary() {
    string output;
    HTTPClient::Option option;
    option.timeout = 200000;
    option.max_data_size = 100 * 1024 * 1024;  // 100MB
    // System dictionary data is in the user's Extensions directory.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    const string data_file_name = "./zipped_data_chromeos";
#else  // GOOGLE_JAPANESE_INPUT_BUILD
    const string data_file_name = "./zipped_data_oss";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    CHECK(HTTPClient::Get(data_file_name, option, &output));
    scoped_ptr<mozc::packed::PackedDataManager>
        data_manager(new mozc::packed::PackedDataManager());
    CHECK(data_manager->InitWithZippedData(output));
    mozc::packed::RegisterPackedDataManager(data_manager.release());
  }

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  // Returns BigDictionaryState
  //   0x00: Correct version BigDictionary is found.
  //   0x1-: BigDictionary is not found.
  //   0x2-: BigDictionary version miss match.
  //   0x-*: Status of the downloader.
  int GetBigDictionaryState() {
    if (big_dictionary_version_ == Version::GetMozcVersion()) {
      return 0;
    }
    int status = 0;
    if (big_dictionary_version_.empty()) {
      status = 0x10;
    } else {
      status = 0x20;
    }
    if (downloader_.get()) {
      status += downloader_->GetStatus();
    }
    return status;
  }

  static bool SendUsageStats(void *data) {
    MozcSessionHandlerThread *self =
        static_cast<MozcSessionHandlerThread *>(data);
    usage_stats::UsageStats::SetInteger("BigDictionaryState",
                                        self->GetBigDictionaryState());
    return usage_stats::UsageStatsUploader::Send(NULL);
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  void GetPosList(Json::Value *response) {
    (*response)["event"]["posList"] = Json::Value(Json::arrayValue);
    Json::Value *pos_list = &(*response)["event"]["posList"];
    vector<string> tmp_pos_vec;
    user_pos_->GetPOSList(&tmp_pos_vec);
    for (int i = 0; i < tmp_pos_vec.size(); ++i) {
      (*pos_list)[i] = Json::Value(Json::objectValue);
      const user_dictionary::UserDictionary::PosType pos_type =
          UserDictionaryUtil::ToPosType(tmp_pos_vec[i].c_str());
      (*pos_list)[i]["type"] =
          Json::Value(user_dictionary::UserDictionary::PosType_Name(pos_type));
      (*pos_list)[i]["name"] = Json::Value(tmp_pos_vec[i]);
    }
  }

  void IsValidReading(const Json::Value &event, Json::Value *response) {
    if (!event.isMember("data")) {
      (*response)["event"]["result"] = false;
      return;
    }
    (*response)["event"]["data"] = event["data"].asString();
    (*response)["event"]["result"] =
        UserDictionaryUtil::IsValidReading(event["data"].asString());
  }

  pp::Instance *instance_;
  BlockingQueue<Json::Value *> *message_queue_;
  pp::CompletionCallbackFactory<MozcSessionHandlerThread> factory_;
  scoped_ptr<EngineInterface> engine_;
  scoped_ptr<SessionHandlerInterface> handler_;
  scoped_ptr<const UserPOSInterface> user_pos_;
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  scoped_ptr<SessionUsageObserver> usage_observer_;
  scoped_ptr<chrome::nacl::DictionaryDownloader> downloader_;
  string big_dictionary_version_;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  DISALLOW_COPY_AND_ASSIGN(MozcSessionHandlerThread);
};

class NaclSessionHandlerInstance : public pp::Instance {
 public:
  explicit NaclSessionHandlerInstance(PP_Instance instance);
  virtual ~NaclSessionHandlerInstance() {}
  virtual void HandleMessage(const pp::Var &var_message);

 private:
  void CheckStatusAndStartMozcSessionHandlerThread();

  scoped_ptr<MozcSessionHandlerThread> mozc_thread_;
  BlockingQueue<Json::Value *> message_queue_;

  DISALLOW_COPY_AND_ASSIGN(NaclSessionHandlerInstance);
};

NaclSessionHandlerInstance::NaclSessionHandlerInstance(PP_Instance instance)
    : pp::Instance(instance) {
  mozc_thread_.reset(new MozcSessionHandlerThread(this, &message_queue_));
  mozc_thread_->Start();
}

void NaclSessionHandlerInstance::HandleMessage(const pp::Var &var_message) {
  if (!var_message.is_string()) {
    return;
  }

  scoped_ptr<Json::Value> message(new Json::Value);
  if (Json::Reader().parse(var_message.AsString(), *message.get())) {
    message_queue_.put(message.release());
  }
}

class NaclSessionHandlerModule : public pp::Module {
 public:
  NaclSessionHandlerModule() : pp::Module() {
  }
  virtual ~NaclSessionHandlerModule() {}

 protected:
  virtual pp::Instance *CreateInstance(PP_Instance instance) {
    return new NaclSessionHandlerInstance(instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaclSessionHandlerModule);
};

}  // namespace session
}  // namespace mozc

namespace pp {

Module *CreateModule() {
  // We use dummy argc and argv to call InitGoogle().
  int argc = 1;
  char argv0[] = "NaclModule";
  char *argv_body[] = {argv0, NULL};
  char **argv = argv_body;
  InitGoogle(argv[0], &argc, &argv, true);

  return new mozc::session::NaclSessionHandlerModule();
}

}  // namespace pp
