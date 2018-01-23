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

// TODO(horo): write tests.

#ifdef OS_NACL

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <cstring>
#include <memory>
#include <queue>
#include <string>

#include "base/clock.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/pepper_file_util.h"
#include "base/port.h"
#include "base/scheduler.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/version.h"
#include "chrome/nacl/dictionary_downloader.h"
#include "config/config_handler.h"
#include "data_manager/data_manager.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "engine/engine.h"
#include "net/http_client.h"
#include "net/http_client_pepper.h"
#include "net/json_util.h"
#include "net/jsoncpp.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
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
  std::queue<T> queue_;

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

  ~MozcSessionHandlerThread() override = default;

  void Run() override {
    Util::SetRandomSeed(static_cast<uint32>(Clock::GetTime()));
    RegisterPepperInstanceForHTTPClient(instance_);

    std::unique_ptr<DataManager> data_manager;
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    data_manager_status_ = DataManager::Status::UNKNOWN;
    const bool filesystem_available =
        PepperFileUtil::Initialize(instance_, kFileIoFileSystemExpectedSize);
    if (!filesystem_available) {
      // Pepper file system is not available, so ignore the big dictionary and
      // use the small dictionary.
      data_manager = LoadDictionary(&data_manager_model_data_buffer_);
    } else {
      data_manager = LoadBigDictionary(&data_manager_status_);
      if (data_manager_status_ != DataManager::Status::OK) {
        LOG_IF(ERROR, data_manager_status_ != DataManager::Status::MMAP_FAILURE)
            << "Failed to load big dictionary: "
            << DataManager::StatusCodeToString(data_manager_status_);
        LOG(INFO) << "Big dictionary is to be downloaded";
        StartDownloadDictionary();
        data_manager = LoadDictionary(&data_manager_model_data_buffer_);
      }
    }
#else  // GOOGLE_JAPANESE_INPUT_BUILD
    PepperFileUtil::Initialize(instance_, kFileIoFileSystemExpectedSize);
    data_manager = LoadDictionary(&data_manager_model_data_buffer_);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    CHECK(data_manager)
        << "Unexpected failure: Data manager shoulnd't be nullptr";

    user_pos_.reset(dictionary::UserPOS::CreateFromDataManager(*data_manager));
    handler_.reset(new SessionHandler(
        mozc::Engine::CreateDesktopEngine(std::move(data_manager))));

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
    message["event"]["data_version"] =
        handler_->engine().GetDataVersion().as_string();

    pp::Module::Get()->core()->CallOnMainThread(
        0,
        factory_.NewCallback(
            &MozcSessionHandlerThread::PostMessage,
            Json::FastWriter().write(message)));

    while (true) {
      bool stopped = false;
      std::unique_ptr<Json::Value> message;
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
          response["event"]["data_version"] =
              handler_->engine().GetDataVersion().as_string();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
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
  static std::unique_ptr<DataManager> LoadBigDictionary(
      DataManager::Status *status) {
    std::unique_ptr<DataManager> data_manager(new DataManager());
    // The big dictionary data is in the user's HTML5 file system.
    *status = data_manager->InitFromFile("/mozc.data");
    return data_manager;
  }

  // Starts downloading the big dictionary.
  void StartDownloadDictionary() {
    downloader_.reset(new chrome::nacl::DictionaryDownloader(
        Version::GetMozcNaclDictionaryUrl(),
        "/mozc.data"));
    downloader_->SetOption(10 * 60 * 1000,  // 10 minutes start delay
                           20 * 60 * 1000,  // + [0-20] minutes random delay
                           30 * 60 * 1000,  // retry_interval 30 min
                           4,  // retry interval [30, 60, 120, 240, 240, 240...]
                           10);  // 10 retries
    downloader_->StartDownload();
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  // Loads the dictionary.
  static std::unique_ptr<DataManager> LoadDictionary(
      std::unique_ptr<uint64> *data_buffer) {
    HTTPClient::Option option;
    option.timeout = 200000;
    option.max_data_size = 100 * 1024 * 1024;  // 100MB
    // System dictionary data is in the user's Extensions directory.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
    const string data_file_name = "./zipped_data_chromeos";
#else  // GOOGLE_JAPANESE_INPUT_BUILD
    const string data_file_name = "./mozc.data";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    string file_content;
    CHECK(HTTPClient::Get(data_file_name, option, &file_content))
        << "Failed to read the content of " << data_file_name;

    // Align the data image at 64 bit boundary.
    const size_t u64_buf_size = (file_content.size() + 7) / 8;
    data_buffer->reset(new uint64[u64_buf_size]);
    memcpy(data_buffer->get(), file_content.data(), file_content.size());
    const StringPiece data(reinterpret_cast<const char *>(data_buffer->get()),
                           file_content.size());
    std::unique_ptr<DataManager> data_manager(new mozc::DataManager());
    const DataManager::Status status = data_manager->InitFromArray(data);
    CHECK_EQ(status, DataManager::Status::OK)
        << "Failed to load " << data_file_name << ": "
        << DataManager::StatusCodeToString(status);
    return data_manager;
  }

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  // Returns BigDictionaryState
  //   0x00: Correct version of BigDictionary is found.
  //   0x1-: BigDictionary is not found.
  //   0x2-: BigDictionary version mismatch.
  //   0x3-: BigDictionary misses some data.
  //   0x4-: BigDictionary is broken.
  //   0x5-: Unknown error.
  //   0x-*: Status of the downloader.
  int GetBigDictionaryState() {
    int status = 0x00;
    switch (data_manager_status_) {
      case DataManager::Status::OK:
        return 0x00;
      case DataManager::Status::MMAP_FAILURE:
        status = 0x10;
        break;
      case DataManager::Status::ENGINE_VERSION_MISMATCH:
        status = 0x20;
        break;
      case DataManager::Status::DATA_MISSING:
        status = 0x30;
        break;
      case DataManager::Status::DATA_BROKEN:
        status = 0x40;
        break;
      default:
        status = 0x50;
        break;
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
    std::vector<string> tmp_pos_vec;
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
  std::unique_ptr<SessionHandler> handler_;
  std::unique_ptr<const UserPOSInterface> user_pos_;
  std::unique_ptr<uint64> data_manager_model_data_buffer_;
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  std::unique_ptr<SessionUsageObserver> usage_observer_;
  std::unique_ptr<chrome::nacl::DictionaryDownloader> downloader_;
  DataManager::Status data_manager_status_;
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

  std::unique_ptr<MozcSessionHandlerThread> mozc_thread_;
  BlockingQueue<Json::Value *> message_queue_;

  DISALLOW_COPY_AND_ASSIGN(NaclSessionHandlerInstance);
};

NaclSessionHandlerInstance::NaclSessionHandlerInstance(PP_Instance instance)
    : pp::Instance(instance) {
  mozc_thread_.reset(new MozcSessionHandlerThread(this, &message_queue_));
  mozc_thread_->Start("NaclSessionHandler");
}

void NaclSessionHandlerInstance::HandleMessage(const pp::Var &var_message) {
  if (!var_message.is_string()) {
    return;
  }

  std::unique_ptr<Json::Value> message(new Json::Value);
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
  // We use dummy argc and argv to call mozc::InitMozc().
  int argc = 1;
  char argv0[] = "NaclModule";
  char *argv_body[] = {argv0, NULL};
  char **argv = argv_body;
  mozc::InitMozc(argv[0], &argc, &argv, true);

  return new mozc::session::NaclSessionHandlerModule();
}

}  // namespace pp

#endif  // OS_NACL
