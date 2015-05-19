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

// TODO(horo): write tests.

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>
#include <ppapi/utility/completion_callback_factory.h>

#include <queue>
#include <string>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/nacl_js_proxy.h"
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
#include "engine/engine_interface.h"
#include "net/http_client.h"
#include "net/http_client_pepper.h"
#include "net/jsoncpp.h"
#include "net/json_util.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#ifdef ENABLE_CLOUD_SYNC
#include "sync/sync_handler.h"
#endif  // ENABLE_CLOUD_SYNC
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
    LoadDictionary();
    user_pos_.reset(
        new UserPOS(
            packed::PackedDataManager::GetUserPosManager()->GetUserPOSData()));

    engine_.reset(mozc::EngineFactory::Create());
    session_factory_.reset(new JapaneseSessionFactory(engine_.get()));
    SessionFactoryManager::SetSessionFactory(session_factory_.get());
    handler_.reset(new SessionHandler());


#ifdef ENABLE_CLOUD_SYNC
    sync_handler_.reset(new sync::SyncHandler);
    handler_->SetSyncHandler(sync_handler_.get());
    Scheduler::AddJob(sync_handler_->GetSchedulerJobSetting());
    uint64 last_reload_required_timestamp = 0;
#endif  // ENABLE_CLOUD_SYNC

    // Gets the current config.
    config::Config config;
    config::ConfigHandler::GetStoredConfig(&config);
    // Sends "InitializeDone" message to JavaScript side code.
    Json::Value message(Json::objectValue);
    message["event"] = Json::objectValue;
    message["event"]["type"] = "InitializeDone";
    JsonUtil::ProtobufMessageToJsonValue(config, &message["event"]["config"]);
    message["event"]["version"] = Version::GetMozcVersion();

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
#ifdef ENABLE_CLOUD_SYNC
      uint64 reload_required_timestamp =
          sync_handler_->GetReloadRequiredTimestamp();
      if (last_reload_required_timestamp != reload_required_timestamp) {
        last_reload_required_timestamp = reload_required_timestamp;
        commands::Command command;
        command.mutable_input()->set_type(commands::Input::RELOAD);
        handler_->EvalCommand(&command);
      }
#endif  // ENABLE_CLOUD_SYNC
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
        } else if (event_type == "GetPosList") {
          GetPosList(&response);
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

  // Loads the dictionary.
  void LoadDictionary() {
    string output;
    HTTPClient::Option option;
    option.timeout = 200000;
    option.max_data_size = 100 * 1024 * 1024;  // 100MB
    // System dictionary data is in the user's Extensions directory.
    string data_file_name = "./zipped_data_oss";
    CHECK(HTTPClient::Get(data_file_name, option, &output));
    scoped_ptr<mozc::packed::PackedDataManager>
        data_manager(new mozc::packed::PackedDataManager());
    CHECK(data_manager->InitWithZippedData(output));
    mozc::packed::RegisterPackedDataManager(data_manager.release());
  }


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

  pp::Instance *instance_;
  BlockingQueue<Json::Value *> *message_queue_;
  pp::CompletionCallbackFactory<MozcSessionHandlerThread> factory_;
  scoped_ptr<EngineInterface> engine_;
  scoped_ptr<SessionHandlerInterface> handler_;
  scoped_ptr<JapaneseSessionFactory> session_factory_;
#ifdef ENABLE_CLOUD_SYNC
  scoped_ptr<sync::SyncHandler> sync_handler_;
#endif  // ENABLE_CLOUD_SYNC
  scoped_ptr<const UserPOSInterface> user_pos_;
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
  NaclJsProxy::Initialize(this);
  mozc_thread_.reset(new MozcSessionHandlerThread(this, &message_queue_));
  mozc_thread_->Start();
}

void NaclSessionHandlerInstance::HandleMessage(const pp::Var &var_message) {
  if (!var_message.is_string()) {
    return;
  }

  scoped_ptr<Json::Value> message(new Json::Value);
  if (Json::Reader().parse(var_message.AsString(), *message.get())) {
    if (message->isMember("jscall")) {
      NaclJsProxy::OnProxyCallResult(message.release());
    } else {
      message_queue_.put(message.release());
    }
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
