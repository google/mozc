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

#include "renderer/renderer_client.h"

#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/process.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/version.h"
#include "base/vlog.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "protocol/renderer_command.pb.h"

#ifdef __APPLE__
#include "base/mac/mac_util.h"
#endif  // __APPLE__

#ifdef _WIN32
#include "base/run_level.h"
#include "base/win32/win_sandbox.h"
#endif  // _WIN32

namespace mozc {
namespace renderer {
namespace {

constexpr absl::Duration kIpcTimeout = absl::Milliseconds(100);
constexpr absl::Duration kRendererWaitTimeout = absl::Seconds(30);
constexpr absl::Duration kRendererWaitSleepTime = absl::Seconds(10);
constexpr size_t kMaxErrorTimes = 5;
constexpr absl::Duration kRetryIntervalTime = absl::Seconds(30);
constexpr char kServiceName[] = "renderer";

inline void CallCommand(IPCClientInterface* client,
                        const commands::RendererCommand& command) {
  std::string buf;
  command.SerializeToString(&buf);

  // basically, we don't need to get the result
  std::string result;

  if (!client->Call(buf, &result, kIpcTimeout)) {
    LOG(ERROR) << "Cannot send the request: ";
  }
}
}  // namespace

class RendererLauncher : public RendererLauncherInterface {
 public:
  RendererLauncher() = default;

  ~RendererLauncher() override {
    if (!launcher_.has_value()) {
      // StartRenderer has never been called.
      return;
    }

    if (!launcher_->Ready()) {
      NamedEventNotifier notify(name_.c_str());
      notify.Notify();
    }
    launcher_->Wait();
  }

  void StartRenderer(
      const std::string& name, const std::string& path,
      bool disable_renderer_path_check,
      IPCClientFactoryInterface* ipc_client_factory_interface) override {
    if (Status() == RendererStatus::RENDERER_LAUNCHING ||
        Status() == RendererStatus::RENDERER_READY ||
        Status() == RendererStatus::RENDERER_TIMEOUT) {
      // Renderer is already launching.
      // The renderer is still up and running when in the pending command state.
      return;
    }
    SetStatus(RendererStatus::RENDERER_LAUNCHING);
    name_ = name;
    path_ = path;
    disable_renderer_path_check_ = disable_renderer_path_check;
    ipc_client_factory_interface_ = ipc_client_factory_interface;
    if (launcher_.has_value()) {
      launcher_->Wait();
    }
    launcher_.emplace([this] { ThreadMain(); });
  }

  bool ForceTerminateRenderer(const std::string& name) override {
    return IPCClient::TerminateServer(name);
  }

  void OnFatal(RendererErrorType type) override {
    LOG(ERROR) << "OnFatal is called: " << static_cast<int>(type);

    std::string error_type;
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

    if (!suppress_error_dialog_) {
      Process::LaunchErrorMessageDialog(error_type);
    }
  }

  bool IsAvailable() const override {
    return (Status() == RendererStatus::RENDERER_READY);
  }

  bool CanConnect() const override {
    switch (Status()) {
      case RendererStatus::RENDERER_UNKNOWN:
      case RendererStatus::RENDERER_READY:
        return true;
      case RendererStatus::RENDERER_LAUNCHING:
        MOZC_VLOG(1) << "now starting renderer";
        return false;
      case RendererStatus::RENDERER_TIMEOUT:
      case RendererStatus::RENDERER_TERMINATED:
        if (error_times_ <= kMaxErrorTimes &&
            Clock::GetAbslTime() - last_launch_time_ >= kRetryIntervalTime) {
          return true;
        }
        MOZC_VLOG(1) << "never re-launch renderer";
        return false;
      case RendererStatus::RENDERER_FATAL:
        MOZC_VLOG(1) << "never re-launch renderer";
        return false;
      default:
        LOG(ERROR) << "Unknown status";
        return false;
    }

    return false;
  }

  void SetPendingCommand(const commands::RendererCommand& command) override
      ABSL_LOCKS_EXCLUDED(mu_) {
    // ignore NOOP|SHUTDOWN
    if (command.type() == commands::RendererCommand::UPDATE) {
      // TODO(b/438604511): Use mu_ (w/o &) when Abseil LTS supports it.
      absl::MutexLock l(&mu_);
      if (!pending_command_.has_value()) {
        pending_command_ = command;
      }
    }
  }

  void set_suppress_error_dialog(bool suppress) override {
    suppress_error_dialog_ = suppress;
  }

 private:
  enum class RendererStatus {
    RENDERER_UNKNOWN = 0,
    RENDERER_LAUNCHING = 1,
    RENDERER_READY = 2,
    RENDERER_TIMEOUT = 3,
    RENDERER_TERMINATED = 4,
    RENDERER_FATAL = 5,
  };

  RendererStatus Status() const ABSL_LOCKS_EXCLUDED(mu_) {
    // TODO(b/438604511): Use mu_ (w/o &) when Abseil LTS supports it.
    absl::MutexLock l(&mu_);
    return renderer_status_;
  }

  void SetStatus(RendererStatus status) ABSL_LOCKS_EXCLUDED(mu_) {
    // TODO(b/438604511): Use mu_ (w/o &) when Abseil LTS supports it.
    absl::MutexLock l(&mu_);
    renderer_status_ = status;
  }

  void ThreadMain() {
    last_launch_time_ = Clock::GetAbslTime();

    NamedEventListener listener(name_.c_str());
    const bool listener_is_available = listener.IsAvailable();

#ifdef _WIN32
    DWORD pid = 0;
    const bool process_in_job = RunLevel::IsProcessInJob();
    const std::string arg = process_in_job ? "--restricted" : "";

    WinSandbox::SecurityInfo info;
    info.primary_level = WinSandbox::USER_INTERACTIVE;
    info.impersonation_level = WinSandbox::USER_RESTRICTED_SAME_ACCESS;
    info.integrity_level = WinSandbox::INTEGRITY_LEVEL_LOW;
    // If the current process is in a job, you cannot use
    // CREATE_BREAKAWAY_FROM_JOB. b/1571395
    info.use_locked_down_job = !process_in_job;
    info.allow_ui_operation = true;  // skip UI protection
    info.in_system_dir = true;  // use system dir not to lock current directory
    info.creation_flags = CREATE_DEFAULT_ERROR_MODE;

    // start renderer process
    const bool result =
        WinSandbox::SpawnSandboxedProcess(path_, arg, info, &pid);
#elif defined(__APPLE__)  // _WIN32
    // Start renderer process by using launch_msg API.
    pid_t pid = 0;
    const bool result = MacUtil::StartLaunchdService("Renderer", &pid);
#else                     // _WIN32, __APPLE__
    size_t tmp = 0;
    const bool result = Process::SpawnProcess(path_, "", &tmp);
    uint32_t pid = static_cast<uint32_t>(tmp);
#endif                    // _WIN32, __APPLE__

    if (!result) {
      LOG(ERROR) << "Can't start process";
      SetStatus(RendererStatus::RENDERER_FATAL);
      return;
    }

    if (listener_is_available) {
      const int ret = listener.WaitEventOrProcess(kRendererWaitTimeout, pid);
      switch (ret) {
        case NamedEventListener::TIMEOUT:
          LOG(ERROR) << "seems that mozc_renderer is not ready within "
                     << kRendererWaitTimeout << " msec";
          SetStatus(RendererStatus::RENDERER_TIMEOUT);
          ++error_times_;
          break;
        case NamedEventListener::EVENT_SIGNALED:
          MOZC_VLOG(1) << "mozc_renderer is launched successfully within "
                       << kRendererWaitTimeout << " msec";
          FlushPendingCommand();
          error_times_ = 0;
          break;
        case NamedEventListener::PROCESS_SIGNALED:
          LOG(ERROR) << "Mozc renderer is terminated";
          SetStatus(RendererStatus::RENDERER_TERMINATED);
          ++error_times_;
          break;
        default:
          LOG(ERROR) << "Unknown status";
          SetStatus(RendererStatus::RENDERER_FATAL);
          ++error_times_;
      }
    } else {
      LOG(ERROR) << "cannot make NamedEventListener ";
      absl::SleepFor(kRendererWaitSleepTime);
      FlushPendingCommand();
      error_times_ = 0;
    }

    if (Status() == RendererStatus::RENDERER_FATAL) {
      OnFatal(RendererLauncherInterface::RENDERER_FATAL);
    }
  }

  void FlushPendingCommand() ABSL_LOCKS_EXCLUDED(mu_) {
    // TODO(b/438604511): Use mu_ (w/o &) when Abseil LTS supports it.
    absl::MutexLock l(&mu_);
    if (ipc_client_factory_interface_ != nullptr &&
        pending_command_.has_value()) {
      std::unique_ptr<IPCClientInterface> client(CreateIPCClient());
      if (client != nullptr) {
        CallCommand(client.get(), *pending_command_);
      }
    }
    pending_command_.reset();

    // |renderer_status_| is also protected by mutex.
    // Until this method finishes, SetPendingCommand is blocked.
    // RendererClint checks the status AGAIN after SetPendingCommand.
    renderer_status_ = RendererStatus::RENDERER_READY;
    error_times_ = 0;
  }

  std::unique_ptr<IPCClientInterface> CreateIPCClient() const {
    if (ipc_client_factory_interface_ == nullptr) {
      return nullptr;
    }
    if (disable_renderer_path_check_) {
      return ipc_client_factory_interface_->NewClient(name_, "");
    }
    return ipc_client_factory_interface_->NewClient(name_, path_);
  }

  std::string name_;
  std::string path_;
  absl::Time last_launch_time_ = absl::UnixEpoch();
  std::atomic<size_t> error_times_ = 0;
  IPCClientFactoryInterface* ipc_client_factory_interface_ = nullptr;
  mutable absl::Mutex mu_;
  std::optional<commands::RendererCommand> pending_command_
      ABSL_GUARDED_BY(mu_);
  RendererStatus renderer_status_ ABSL_GUARDED_BY(mu_) =
      RendererStatus::RENDERER_UNKNOWN;
  bool disable_renderer_path_check_ = false;
  bool suppress_error_dialog_ = false;
  std::optional<BackgroundFuture<void>> launcher_;
};

RendererClient::RendererClient()
    : is_window_visible_(false),
      disable_renderer_path_check_(false),
      version_mismatch_nums_(0),
      ipc_client_factory_interface_(IPCClientFactory::GetIPCClientFactory()),
      renderer_launcher_(new RendererLauncher),
      renderer_launcher_interface_(nullptr) {
  renderer_launcher_interface_ = renderer_launcher_.get();

  name_ = kServiceName;
  const std::string desktop_name(SystemUtil::GetDesktopNameAsString());
  if (!desktop_name.empty()) {
    name_ += ".";
    name_ += desktop_name;
  }

  renderer_path_ = SystemUtil::GetRendererPath();
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
    IPCClientFactoryInterface* ipc_client_factory_interface) {
  ipc_client_factory_interface_ = ipc_client_factory_interface;
}

void RendererClient::SetRendererLauncherInterface(
    RendererLauncherInterface* renderer_launcher_interface) {
  renderer_launcher_interface_ = renderer_launcher_interface;
}

bool RendererClient::Activate() {
  if (IsAvailable()) {  // already running
    return true;
  }
  commands::RendererCommand command;
  command.set_type(commands::RendererCommand::NOOP);
  return ExecCommand(command);
}

bool RendererClient::IsAvailable() const {
  if (renderer_launcher_interface_ == nullptr) {
    LOG(ERROR) << "renderer_launcher_interface is nullptr";
    return false;
  }
  return renderer_launcher_interface_->IsAvailable();
}

bool RendererClient::Shutdown(bool force) {
  std::unique_ptr<IPCClientInterface> client(CreateIPCClient());

  if (!client) {
    LOG(ERROR) << "Cannot make client object";
    return false;
  }

  if (!client->Connected()) {
    MOZC_VLOG(1) << "renderer is not running.";
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

void RendererClient::set_suppress_error_dialog(bool suppress) {
  if (renderer_launcher_interface_ == nullptr) {
    LOG(ERROR) << "RendererLauncher is nullptr";
    return;
  }
  renderer_launcher_interface_->set_suppress_error_dialog(suppress);
}

bool RendererClient::ExecCommand(const commands::RendererCommand& command) {
  if (renderer_launcher_interface_ == nullptr) {
    LOG(ERROR) << "RendererLauncher is nullptr";
    return false;
  }

  if (ipc_client_factory_interface_ == nullptr) {
    LOG(ERROR) << "IPCClientFactory is nullptr";
    return false;
  }

  if (!renderer_launcher_interface_->CanConnect()) {
    renderer_launcher_interface_->SetPendingCommand(command);
    // Check CanConnect() again, as the status might be changed
    // after SetPendingCommand().
    if (!renderer_launcher_interface_->CanConnect()) {
      MOZC_VLOG(1) << "renderer_launcher::CanConnect() return false";
      return true;
    }
  }

  // Drop the current request if version mismatch happens.
  constexpr int kMaxVersionMismatchNums = 3;
  if (version_mismatch_nums_ >= kMaxVersionMismatchNums) {
    return true;
  }

  MOZC_VLOG(2) << "Sending: " << command;

  std::unique_ptr<IPCClientInterface> client(CreateIPCClient());

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
    LOG(WARNING) << "Version mismatch: " << client->GetServerProductVersion()
                 << " " << Version::GetMozcVersion();
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

std::unique_ptr<IPCClientInterface> RendererClient::CreateIPCClient() const {
  if (ipc_client_factory_interface_ == nullptr) {
    return nullptr;
  }
  if (disable_renderer_path_check_) {
    return ipc_client_factory_interface_->NewClient(name_, "");
  }
  return ipc_client_factory_interface_->NewClient(name_, renderer_path_);
}

}  // namespace renderer
}  // namespace mozc
