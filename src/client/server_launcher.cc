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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/const.h"
#include "base/file_stream.h"  // IWYU pragma: keep, for debug build
#include "base/file_util.h"  // IWYU pragma: keep, for debug build
#include "base/process.h"
#include "base/system_util.h"
#include "base/vlog.h"
#include "client/client.h"
#include "client/client_interface.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"

#ifdef __APPLE__
#include "base/mac/mac_util.h"
#endif  // __APPLE__

#ifdef _WIN32
#include <sddl.h>
#include <shlobj.h>
#include <windows.h>

#include "base/run_level.h"
#include "base/win32/win_sandbox.h"
#else  // _WIN32
#include <unistd.h>
#endif  // _WIN32

namespace mozc {
namespace client {
namespace {
constexpr char kServerName[] = "session";

// Wait at most kServerWaitTimeout msec until server gets ready
constexpr absl::Duration kServerWaitTimeout = absl::Seconds(20);

// for every 1000m sec, check server
constexpr absl::Duration kRetryIntervalForServer = absl::Milliseconds(1000);

// Try 20 times to check mozc_server is running
constexpr uint32_t kTrial = 20;

#ifdef DEBUG
// Load special flags for server.
// This should be enabled on debug build
const std::string LoadServerFlags() {
  constexpr char kServerFlagsFile[] = "mozc_server_flags.txt";
  const std::string filename = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), kServerFlagsFile);
  std::string flags;
  InputFileStream ifs(filename);
  if (ifs) {
    getline(ifs, flags);
  }
  MOZC_VLOG(1) << "New server flag: " << flags;
  return flags;
}
#endif  // DEBUG
}  // namespace

// initialize default path
ServerLauncher::ServerLauncher()
    : server_program_(SystemUtil::GetServerPath()),
      suppress_error_dialog_(false) {}

ServerLauncher::~ServerLauncher() = default;

bool ServerLauncher::StartServer(ClientInterface *client) {
  if (server_program().empty()) {
    LOG(ERROR) << "Server path is empty";
    return false;
  }

  // ping first
  if (client->PingServer()) {
    return true;
  }

  std::string arg;

#ifdef _WIN32
  // When mozc is not used as a default IME and some applications (like notepad)
  // are registered in "Start up", mozc_server may not be launched successfully.
  // This is because the Explorer launches start-up processes inside a group job
  // and the process inside a job cannot make our sandboxed child processes.
  // The group job is unregistered after 60 secs (default).
  //
  // Here we relax the sandbox restriction if process is in a job.
  // In order to keep security, the mozc_server is launched
  // with restricted mode.

  const bool process_in_job = RunLevel::IsProcessInJob();
  if (process_in_job) {
    LOG(WARNING) << "Parent process is in job. start with restricted mode";
    arg += "--restricted";
  }
#endif  // _WIN32

#ifdef DEBUG
  // In order to test the Session treatment (timeout/size constraints),
  // Server flags can be configurable on DEBUG build
  if (!arg.empty()) {
    arg += " ";
  }
  arg += LoadServerFlags();
#endif  // DEBUG

  NamedEventListener listener(kServerName);
  const bool listener_is_available = listener.IsAvailable();

  size_t pid = 0;
#ifdef _WIN32
  mozc::WinSandbox::SecurityInfo info;
  // You cannot use WinSandbox::USER_INTERACTIVE here because restricted token
  // seems to prevent WinHTTP from using SSL. b/5502343
  info.primary_level = WinSandbox::USER_NON_ADMIN;
  info.impersonation_level = WinSandbox::USER_RESTRICTED_SAME_ACCESS;
  info.integrity_level = WinSandbox::INTEGRITY_LEVEL_LOW;
  // If the current process is in a job, you cannot use
  // CREATE_BREAKAWAY_FROM_JOB. b/1571395
  info.use_locked_down_job = !process_in_job;
  info.allow_ui_operation = false;
  info.in_system_dir = true;  // use system dir not to lock current directory
  info.creation_flags = CREATE_DEFAULT_ERROR_MODE | CREATE_NO_WINDOW;

  DWORD tmp_pid = 0;
  const bool result = mozc::WinSandbox::SpawnSandboxedProcess(
      server_program(), arg, info, &tmp_pid);
  pid = static_cast<size_t>(tmp_pid);

  if (!result) {
    LOG(ERROR) << "Can't start process: " << ::GetLastError();
    return false;
  }
#elif defined(__APPLE__)  // _WIN32
  // Use launchd API instead of spawning process.  It doesn't use
  // server_program() at all.
  const bool result = MacUtil::StartLaunchdService(
      "Converter", reinterpret_cast<pid_t *>(&pid));
  if (!result) {
    LOG(ERROR) << "Can't start process";
    return false;
  }
#else                     // defined(__APPLE__)
  const bool result = mozc::Process::SpawnProcess(server_program(), arg, &pid);
  if (!result) {
    LOG(ERROR) << "Can't start process: " << strerror(result);
    return false;
  }
#endif                    // defined(__APPLE__)

  // maybe another process will launch mozc_server at the same time.
  if (client->PingServer()) {
    MOZC_VLOG(1) << "Another process has launched the server";
    return true;
  }

  // Common part:
  // Wait until mozc_server becomes ready to process requests
  if (listener_is_available) {
    const int ret = listener.WaitEventOrProcess(kServerWaitTimeout, pid);
    switch (ret) {
      case NamedEventListener::TIMEOUT:
        LOG(WARNING) << "seems that " << kProductNameInEnglish << " is not "
                     << "ready within " << kServerWaitTimeout << " msec";
        break;
      case NamedEventListener::EVENT_SIGNALED:
        MOZC_VLOG(1) << kProductNameInEnglish << " is launched successfully "
                     << "within " << kServerWaitTimeout << " msec";
        break;
      case NamedEventListener::PROCESS_SIGNALED:
        LOG(ERROR) << "Mozc server is terminated";
        // Mozc may be terminated because another client launches mozc_server
        if (client->PingServer()) {
          return true;
        }
        return false;
    }
  } else {
    // maybe another process is trying to launch mozc_server.
    LOG(ERROR) << "cannot make NamedEventListener ";
    absl::SleepFor(kRetryIntervalForServer);
  }

  // Try to connect mozc_server just in case.
  for (int trial = 0; trial < kTrial; ++trial) {
    if (client->PingServer()) {
      return true;
    }
    absl::SleepFor(kRetryIntervalForServer);
  }

  LOG(ERROR) << kProductNameInEnglish << " cannot be launched";

  return false;
}

bool ServerLauncher::ForceTerminateServer(const absl::string_view name) {
  return IPCClient::TerminateServer(name);
}

bool ServerLauncher::WaitServer(uint32_t pid) {
  constexpr int kTimeout = 10000;
  return Process::WaitProcess(static_cast<size_t>(pid), kTimeout);
}

void ServerLauncher::OnFatal(ServerLauncherInterface::ServerErrorType type) {
  LOG(ERROR) << "OnFatal is called: " << static_cast<int>(type);

  std::string error_type;
  switch (type) {
    case ServerLauncherInterface::SERVER_TIMEOUT:
      error_type = "server_timeout";
      break;
    case ServerLauncherInterface::SERVER_BROKEN_MESSAGE:
      error_type = "server_broken_message";
      break;
    case ServerLauncherInterface::SERVER_VERSION_MISMATCH:
      error_type = "server_version_mismatch";
      break;
    case ServerLauncherInterface::SERVER_SHUTDOWN:
      error_type = "server_shutdown";
      break;
    case ServerLauncherInterface::SERVER_FATAL:
      error_type = "server_fatal";
      break;
    default:
      LOG(ERROR) << "Unknown error: " << type;
      return;
  }

  if (!suppress_error_dialog_) {
    Process::LaunchErrorMessageDialog(error_type);
  }
}
}  // namespace client
}  // namespace mozc
