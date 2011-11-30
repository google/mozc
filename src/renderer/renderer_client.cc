// Copyright 2010-2011, Google Inc.
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

#include "renderer/renderer_client.h"

#include <climits>
#include <string>
#include "base/base.h"
#include "base/const.h"
#include "base/mutex.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "session/commands.pb.h"

#ifdef OS_MACOSX
#include "base/mac_util.h"
#endif

namespace mozc {
namespace renderer {
namespace {

const int    kIPCTimeout            = 100;        // 100 msec
const int    kRendererWaitTimeout   = 30 * 1000;  // 30 sec
const int    kRendererWaitSleepTime = 10 * 1000;  // 10 sec
const size_t kMaxErrorTimes         = 5;
const uint64 kRetryIntervalTime     = 30;  // 30 sec
const char   kServiceName[]         = "renderer";

#if !defined(OS_WINDOWS)
const char kMozcRenderer[] = "mozc_renderer";
#endif  // !OS_WINDOWS

// TODO(taku): move it to Util
string GetRendererPath() {
  return mozc::Util::JoinPath(mozc::Util::GetServerDirectory(),
#if defined(OS_WINDOWS)
                              mozc::kMozcRenderer);
#else  // OS_WINDOWS
                              kMozcRenderer);
#endif
}

inline void CallCommand(IPCClientInterface *client,
                        const commands::RendererCommand &command) {
  string buf;
  command.SerializeToString(&buf);

  // basically, we don't need to get the result
  char result[32];
  size_t result_size = sizeof(result);

  if (!client->Call(buf.data(), buf.size(),
                    result, &result_size,
                    kIPCTimeout)) {
    LOG(ERROR) << "Cannot send the request: ";
  }
}
}  // namespace

class RendererLauncher : public RendererLauncherInterface,
                         public Thread {
 public:
  bool CanConnect() const {
    switch (renderer_status_) {
      case RendererLauncher::RENDERER_UNKNOWN:
      case RendererLauncher::RENDERER_READY:
        return true;
      case RendererLauncher::RENDERER_LAUNCHING:
        VLOG(1) << "now starting renderer";
        return false;
      case RendererLauncher::RENDERER_TIMEOUT:
      case RendererLauncher::RENDERER_TERMINATED:
        if (error_times_ <= kMaxErrorTimes &&
            Util::GetTime() - last_launch_time_ >= kRetryIntervalTime) {
          return true;
        }
        VLOG(1) << "never re-launch renderer";
        return false;
      case RendererLauncher::RENDERER_FATAL:
        VLOG(1) << "never re-launch renderer";
        return false;
      default:
        LOG(ERROR) << "Unknown status";
        return false;
    }

    return false;
  }

  bool IsAvailable() const {
    return (renderer_status_ == RendererLauncher::RENDERER_READY);
  }

  void StartRenderer(const string &name, const string &path,
                     bool disable_renderer_path_check,
                     IPCClientFactoryInterface *ipc_client_factory_interface) {
    renderer_status_ = RendererLauncher::RENDERER_LAUNCHING;
    name_ = name;
    path_ = path;
    disable_renderer_path_check_ = disable_renderer_path_check;
    ipc_client_factory_interface_ = ipc_client_factory_interface;
    Thread::Start();
  }

  void Run() {
    last_launch_time_ = Util::GetTime();

    NamedEventListener listener(name_.c_str());
    const bool listener_is_available = listener.IsAvailable();

#ifdef OS_WINDOWS
    DWORD pid = 0;
    const bool process_in_job = RunLevel::IsProcessInJob();
    const string arg = process_in_job ? "--restricted" : "";

    WinSandbox::SecurityInfo info;
    info.primary_level = WinSandbox::USER_INTERACTIVE;
    info.impersonation_level = WinSandbox::USER_RESTRICTED_SAME_ACCESS;
    info.integrity_level = WinSandbox::INTEGRITY_LEVEL_LOW;
    // If the current process is in a job, you cannot use
    // CREATE_BREAKAWAY_FROM_JOB. b/1571395
    info.use_locked_down_job = !process_in_job;
    info.allow_ui_operation = true;   // skip UI protection
    info.in_system_dir = true;  // use system dir not to lock current directory
    info.creation_flags = CREATE_DEFAULT_ERROR_MODE;

    // start renreder process
    const bool result = WinSandbox::SpawnSandboxedProcess(path_, arg, info,
                                                          &pid);
#elif defined(OS_MACOSX)
    // Start renderer process by using launch_msg API.
    pid_t pid = 0;
    const bool result = MacUtil::StartLaunchdService("Renderer", &pid);
#else
    size_t tmp = 0;
    const bool result = Process::SpawnProcess(path_, "", &tmp);
    uint32 pid = static_cast<uint32>(tmp);
#endif

    if (!result) {
      LOG(ERROR) << "Can't start process";
      renderer_status_ = RendererLauncher::RENDERER_FATAL;
      return;
    }

    if (listener_is_available) {
      const int ret = listener.WaitEventOrProcess(kRendererWaitTimeout,
                                                  pid);
      switch (ret) {
        case NamedEventListener::TIMEOUT:
          LOG(ERROR) << "seems that mozc_renderer is not ready within "
                     << kRendererWaitTimeout << " msec";
          renderer_status_ = RendererLauncher::RENDERER_TIMEOUT;
          ++error_times_;
          break;
        case NamedEventListener::EVENT_SIGNALED:
          VLOG(1) << "mozc_renderer is launched successfully within "
                  << kRendererWaitTimeout << " msec";
          FlushPendingCommand();
          renderer_status_ = RendererLauncher::RENDERER_READY;
          error_times_ = 0;
          break;
        case NamedEventListener::PROCESS_SIGNALED:
          LOG(ERROR) << "Mozc renderer is terminated";
          renderer_status_ = RendererLauncher::RENDERER_TERMINATED;
          ++error_times_;
          break;
        default:
          LOG(ERROR) << "Unknown status";
          renderer_status_ = RendererLauncher::RENDERER_FATAL;
          ++error_times_;
      }
    } else {
      LOG(ERROR) << "cannot make NamedEventListener ";
      Util::Sleep(kRendererWaitSleepTime);
      FlushPendingCommand();
      renderer_status_ = RendererLauncher::RENDERER_READY;
      error_times_ = 0;
    }

    if (renderer_status_ == RendererLauncher::RENDERER_FATAL) {
      OnFatal(RendererLauncherInterface::RENDERER_FATAL);
    }
  }

  bool ForceTerminateRenderer(const string &name) {
    return IPCClient::TerminateServer(name);
  }

  void OnFatal(RendererErrorType type) {
    LOG(ERROR) << "OnFatal is called: " << static_cast<int>(type);

    string error_type;
    switch (type) {
      case RendererLauncherInterface::RENDERER_VERSION_MISMATCH:
        error_type = "renderer_version_mismatch";
        break;
      case RendererLauncherInterface::RENDERER_FATAL:
        error_type = "renderer_fatal";
        break;
      default:
        LOG(ERROR) << "Unknown error: " << type;
        return;
    }

    Process::LaunchErrorMessageDialog(error_type);
  }

  void SetPendingCommand(const commands::RendererCommand &command) {
    // ignore NOOP|SHUTDOWN
    if (command.type() == commands::RendererCommand::UPDATE) {
      scoped_lock l(&pending_command_mutex_);
      if (pending_command_.get() == NULL) {
        pending_command_.reset(new commands::RendererCommand);
      }
      pending_command_->CopyFrom(command);
    }
  }

  RendererLauncher()
      : renderer_status_(RendererLauncher::RENDERER_UNKNOWN),
        last_launch_time_(0),
        error_times_(0),
        disable_renderer_path_check_(false),
        ipc_client_factory_interface_(NULL) {}

  virtual ~RendererLauncher() {
    if (!IsRunning()) {
      return;
    }
    NamedEventNotifier notify(name_.c_str());
    notify.Notify();
    Join();
  }

 private:
  enum RendererStatus {
    RENDERER_UNKNOWN    = 0,
    RENDERER_LAUNCHING  = 1,
    RENDERER_READY      = 2,
    RENDERER_TIMEOUT    = 3,
    RENDERER_TERMINATED = 4,
    RENDERER_FATAL      = 5,
  };

  void FlushPendingCommand() {
    scoped_lock l(&pending_command_mutex_);
    if (ipc_client_factory_interface_ != NULL &&
        pending_command_.get() != NULL) {
      scoped_ptr<IPCClientInterface> client(
          ipc_client_factory_interface_->
          NewClient(name_,
                    disable_renderer_path_check_ ? "" : path_));
      CallCommand(client.get(), *(pending_command_.get()));
    }
    pending_command_.reset(NULL);

    // |renderer_status_| is also protected by mutex.
    // Until this method finsihs, SetPendingCommand is blocked.
    // RendererClint checks the status AGAIN after SetPendingCommand.
    renderer_status_ = RendererLauncher::RENDERER_READY;
    error_times_ = 0;
  }

  string name_;
  string path_;
  volatile RendererStatus renderer_status_;
  volatile uint64 last_launch_time_;
  volatile size_t error_times_;
  bool disable_renderer_path_check_;
  IPCClientFactoryInterface *ipc_client_factory_interface_;
  scoped_ptr<commands::RendererCommand> pending_command_;
  Mutex pending_command_mutex_;
};

RendererClient::RendererClient()
    : is_window_visible_(false),
      disable_renderer_path_check_(false),
      version_mismatch_nums_(0),
      ipc_client_factory_interface_(IPCClientFactory::GetIPCClientFactory()),
      renderer_launcher_(new RendererLauncher),
      renderer_launcher_interface_(NULL) {
  renderer_launcher_interface_ = renderer_launcher_.get();

  name_ = kServiceName;
  const string desktop_name(Util::GetDesktopNameAsString());
  if (!desktop_name.empty()) {
    name_ += ".";
    name_ += desktop_name;
  }

  renderer_path_ = GetRendererPath();
}

RendererClient::~RendererClient() {
  if (!IsAvailable() || !is_window_visible_) {
    return;
  }
  commands::RendererCommand command;
  command.set_visible(false);
  command.set_type(commands::RendererCommand::UPDATE);
  ExecCommand(command);
}

void RendererClient::SetIPCClientFactory(
      IPCClientFactoryInterface *ipc_client_factory_interface) {
  ipc_client_factory_interface_ = ipc_client_factory_interface;
}

void RendererClient::SetRendererLauncherInterface(
      RendererLauncherInterface *renderer_launcher_interface) {
  renderer_launcher_interface_ = renderer_launcher_interface;
}

bool RendererClient::Activate() {
  if (IsAvailable()) {   // already running
    return true;
  }
  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  return ExecCommand(command);
}

bool RendererClient::IsAvailable() const {
  if (renderer_launcher_interface_ == NULL) {
    LOG(ERROR) << "renderer_launcher_interface is NULL";
    return false;
  }
  return renderer_launcher_interface_->IsAvailable();
}

bool RendererClient::Shutdown(bool force) {
  scoped_ptr<IPCClientInterface> client(
      ipc_client_factory_interface_->
      NewClient(name_,
                disable_renderer_path_check_ ? "" : renderer_path_));

  if (client.get() == NULL) {
    LOG(ERROR) << "Cannot make client object";
    return false;
  }

  if (!client->Connected()) {
    VLOG(1) << "renderer is not running.";
    return true;
  }

  if (force) {
    if (!renderer_launcher_interface_->ForceTerminateRenderer(name_)) {
      LOG(ERROR) << "ForceTerminateServer failed";
      return false;
    }
  } else {
    commands::RendererCommand command;
    command.set_type(commands::RendererCommand::SHUTDOWN);
    return ExecCommand(command);
  }

  return true;
}

void RendererClient::DisableRendererServerCheck() {
  disable_renderer_path_check_ = true;
}

bool RendererClient::ExecCommand(const commands::RendererCommand &command) {
  if (renderer_launcher_interface_ == NULL) {
    LOG(ERROR) << "RendererLauncher is NULL";
    return false;
  }

  if (ipc_client_factory_interface_ == NULL) {
    LOG(ERROR) << "IPCClientFactory is NULL";
    return false;
  }

  if (!renderer_launcher_interface_->CanConnect()) {
    renderer_launcher_interface_->SetPendingCommand(command);
    // Check CanConnect() again, as the status might be changed
    // after SetPendingCommand().
    if (!renderer_launcher_interface_->CanConnect()) {
      VLOG(1) << "renderer_launcher::CanConnect() return false";
      return true;
    }
  }

  // Drop the current request if version mismatch happens.
  const int kMaxVersionMismatchNums = 3;
  if (version_mismatch_nums_ >= kMaxVersionMismatchNums) {
    return true;
  }

  VLOG(2) << "Sending: " << command.DebugString();

  scoped_ptr<IPCClientInterface> client(
      ipc_client_factory_interface_->
      NewClient(name_,
                disable_renderer_path_check_ ? "" : renderer_path_));

  // In case IPCClient::Init fails with timeout error, the last error should be
  // checked here.  See also b/3264926.
  // TODO(yukawa): Check any other error.
  if (client->GetLastIPCError() == IPC_TIMEOUT_ERROR) {
    return false;
  }

  is_window_visible_ = command.visible();

  if (!client->Connected()) {
    // We don't need to send HIDE if the renderer is not running
    if (command.type() == commands::RendererCommand::UPDATE &&
        (!is_window_visible_ || !command.has_output())) {
      LOG(WARNING) << "Discards a HIDE command since the "
                   << "renderer is not running";
      return true;
    }
    LOG(WARNING) << "cannot connect to renderer. restarting";
    renderer_launcher_interface_->SetPendingCommand(command);
    renderer_launcher_interface_->StartRenderer(name_, renderer_path_,
                                                disable_renderer_path_check_,
                                                ipc_client_factory_interface_);
    return true;
  }

  if (IPC_PROTOCOL_VERSION > client->GetServerProtocolVersion()) {
    LOG(WARNING) << "Protocol version mismatch: "
                 << static_cast<int>(IPC_PROTOCOL_VERSION) << " "
                 << client->GetServerProtocolVersion();
    if (!renderer_launcher_interface_->ForceTerminateRenderer(name_)) {
      LOG(ERROR) << "ForceTerminateServer failed";
    }
    ++version_mismatch_nums_;
    renderer_launcher_interface_->SetPendingCommand(command);
    return true;
  } else if (IPC_PROTOCOL_VERSION < client->GetServerProtocolVersion()) {
    version_mismatch_nums_ = INT_MAX;
    renderer_launcher_interface_->OnFatal(
        RendererLauncherInterface::RENDERER_VERSION_MISMATCH);
    LOG(ERROR) << "client protocol version is older than "
               << "renderer protocol version.";
    return true;
  }

  if (Version::CompareVersion(client->GetServerProductVersion(),
                              Version::GetMozcVersion())) {
    LOG(WARNING) << "Version mismatch: "
                 << client->GetServerProductVersion() << " "
                 << Version::GetMozcVersion();
    renderer_launcher_interface_->SetPendingCommand(command);
    commands::RendererCommand shutdown_command;
    shutdown_command.set_type(commands::RendererCommand::SHUTDOWN);
    CallCommand(client.get(), shutdown_command);
    ++version_mismatch_nums_;
    return true;
  }

  CallCommand(client.get(), command);

  return true;
}
}  // renderer
}  // mozc
