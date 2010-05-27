// Copyright 2010, Google Inc.
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

// A class handling the IPC connection for the session b/w server and clients.

#include "client/session.h"

#include <stdio.h>

#include "base/base.h"
#include "base/const.h"
#include "base/crash_report_util.h"
#include "base/file_stream.h"
#include "base/process.h"
#include "base/run_level.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "session/commands.pb.h"

#ifdef OS_MACOSX
#include "base/mac_process.h"
#endif

namespace mozc {
namespace client {

namespace {
const char kServerAddress[]    = "session"; // name for the IPC connection.
const int    kResultBufferSize = 8192 * 32;   // size of IPC buffer
const size_t kMaxPlayBackSize  = 512;   // size of maximum history

#ifdef _DEBUG
const int kDefaultTimeout = 100000;   // 100 sec for dbg
#else
const int kDefaultTimeout = 30000;    // 30 sec for opt
#endif  // _DEBUG

// Delete Session is called inside the Destructor of Session class.
// To prevent from an application being stalled at the close time,
// we change the timeout of DeleteSession shorter.
// This timeout is only applied in the DeleteSessions command
// called from Destructor. When an application calls DeleteSession
// explicitly, the default timeout is used.
const int kDeleteSessionOnDestructorTimeout = 1000;  // 1 sec
}  // namespace

Session::Session()
    : id_(0),
      server_launcher_(new ServerLauncher),
      result_(new char[kResultBufferSize]),
      preferences_(NULL),
      timeout_(kDefaultTimeout),
      server_status_(SERVER_UNKNOWN),
      server_protocol_version_(0),
      server_process_id_(0) {
  client_factory_ = IPCClientFactory::GetIPCClientFactory();
}

Session::~Session() {
  set_timeout(kDeleteSessionOnDestructorTimeout);
  DeleteSession();
}

void Session::SetIPCClientFactory(IPCClientFactoryInterface *client_factory) {
  client_factory_ = client_factory;
}

void Session::SetServerLauncher(
    ServerLauncherInterface *server_launcher) {
  server_launcher_.reset(server_launcher);
}

bool Session::IsValidRunLevel() const {
  return RunLevel::IsValidClientRunLevel();
}

bool Session::EnsureConnection() {
  switch (server_status_) {
    case SERVER_OK:
    case SERVER_INVALID_SESSION:
      return true;
      break;
    case SERVER_FATAL:
      // once the current status goes into SERVER_FATAL. do nothing.
      return false;
      break;
    case SERVER_TIMEOUT:
      OnFatal(ServerLauncherInterface::SERVER_TIMEOUT);
      server_status_ = SERVER_FATAL;
      return false;
      break;
    case SERVER_BROKEN_MESSAGE:
      OnFatal(ServerLauncherInterface::SERVER_BROKEN_MESSAGE);
      server_status_ = SERVER_FATAL;
      return false;
      break;
    case SERVER_VERSION_MISMATCH:
      OnFatal(ServerLauncherInterface::SERVER_VERSION_MISMATCH);
      server_status_ = SERVER_FATAL;
      return false;
      break;
    case SERVER_SHUTDOWN:
#ifdef _DEBUG
      OnFatal(ServerLauncherInterface::SERVER_SHUTDOWN);
      // don't break here as SERVER_SHUTDOWN and SERVER_UNKNOWN
      // have basically the same treatment.
#endif  // _DEBUG
    case SERVER_UNKNOWN:
      if (StartServer()) {
        server_status_ = SERVER_INVALID_SESSION;
        return true;
      } else {
        LOG(ERROR) << "Cannot start server";
        OnFatal(ServerLauncherInterface::SERVER_FATAL);
        server_status_ = SERVER_FATAL;
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Unknown status: " << server_status_;
      break;
  }

  return true;
}

bool Session::EnsureSession() {
  if (!EnsureConnection()) {
    return false;
  }

  if (server_status_ == SERVER_INVALID_SESSION) {
    if (CreateSession()) {
      server_status_ = SERVER_OK;
      return true;
    } else {
      LOG(ERROR) << "CreateSession failed";
      // call EnsureConnection to display error message
      EnsureConnection();
      return false;
    }
  }

  return true;
}

void Session::DumpQueryOfDeath() {
  LOG(ERROR) << "The playback history looks like a query of death";
  const char kFilename[] = "query_of_death.log";
  const char kLabel[] = "Query of Death";
  DumpHistorySnapshot(kFilename, kLabel);
  history_inputs_.clear();
}

void Session::DumpHistorySnapshot(const string &filename,
                                  const string &label) const {
  const string snapshot_file = Util::JoinPath(Util::GetUserProfileDirectory(),
                                              filename);
  // open with append mode
  OutputFileStream output(snapshot_file.c_str(), ios::app);

  output << "---- Start history snapshot for " << label << endl;
  output << "Created at " << Logging::GetLogMessageHeader() << endl;
  output << "Version " << Version::GetMozcVersion() << endl;
  for (size_t i = 0; i < history_inputs_.size(); ++i) {
    output << history_inputs_[i].DebugString();
  }
  output << "---- End history snapshot for " << label << endl;
}

void Session::PlaybackHistory() {
  if (history_inputs_.size() >= kMaxPlayBackSize) {
    history_inputs_.clear();
    return;
  }

  commands::Output output;
  VLOG(1) << "Playback history: size=" << history_inputs_.size();
  for (size_t i = 0; i < history_inputs_.size(); ++i) {
    history_inputs_[i].set_id(id_);
    if (!Call(history_inputs_[i], &output)) {
      LOG(ERROR) << "playback history failed: "
                 << history_inputs_[i].DebugString();
      break;
    }
  }
}

void Session::PushHistory(const commands::Input &input,
                          const commands::Output &output) {
  // don't insert a new input when history_inputs_.size()
  // reaches to the maximum size. This prevents DOS attack.
  if (history_inputs_.size() < kMaxPlayBackSize) {
    history_inputs_.push_back(input);
  }

  // found context boundary.
  if (input.type() == commands::Input::SEND_KEY &&
      (!output.has_preedit() || output.has_result())) {
    history_inputs_.clear();
  }
}

bool Session::SendKey(const commands::KeyEvent &key,
                      commands::Output *output) {
#ifdef _DEBUG
  if (IsAbortKey(key)) {
    DCHECK(CrashReportUtil::Abort()) << "Not aborted by CrashReportUtil::Abort";
  }
#endif  // _DEBUG
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EnsureCallCommand(&input, output);
}

bool Session::TestSendKey(const commands::KeyEvent &key,
                          commands::Output *output) {
#ifdef _DEBUG
  if (IsAbortKey(key)) {
    DCHECK(CrashReportUtil::Abort()) << "Not aborted by CrashReportUtil::Abort";
  }
#endif  // _DEBUG
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  return EnsureCallCommand(&input, output);
}

bool Session::SendCommand(const commands::SessionCommand &command,
                          commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->CopyFrom(command);
  return EnsureCallCommand(&input, output);
}

bool Session::CheckVersionOrRestartServer() {
  commands::Input input;
  commands::Output output;
  input.set_type(commands::Input::NO_OPERATION);
  if (!CheckVersionOrRestartServerInternal(input, &output)) {
    LOG(ERROR) << "CheckVersionOrRestartServerInternal failed";
    if (!EnsureConnection()) {
      LOG(ERROR) << "EnsureConnection failed";
      return false;
    }
  }

  return true;
}

bool Session::EnsureCallCommand(commands::Input *input,
                                commands::Output *output) {
  if (!EnsureSession()) {
    LOG(ERROR) << "EnsureSession failed";
    return false;
  }

  InitInput(input);
  output->set_id(0);

  if (!CallAndCheckVersion(*input, output)) {  // server is not running
    LOG(ERROR) << "Call command failed";
  } else if (output->id() != input->id()) {   // invalid ID
    LOG(ERROR) << "Session id is void. re-issue session id";
    server_status_ = SERVER_INVALID_SESSION;
  }

  // see the result of Call
  if (server_status_ >= SERVER_TIMEOUT) {
    return false;
  }

  if (server_status_ == SERVER_SHUTDOWN ||
      server_status_ == SERVER_INVALID_SESSION) {
    if (EnsureSession()) {
      // playback the history to restore the previous state.
      PlaybackHistory();
      InitInput(input);
#ifdef _DEBUG
      // The debug binary dumps query of death at the first trial.
      history_inputs_.push_back(*input);
      DumpQueryOfDeath();
#endif  // _DEBUG
      // second trial
      if (!CallAndCheckVersion(*input, output)) {
#ifndef _DEBUG
        // if second trial failed, record the input
        history_inputs_.push_back(*input);
        // Opt or release binaries refrain from dumping query of death
        // at the first trial, but dumps it at the second trial.
        //
        // TODO(komatsu, taku): Should release binary dump query of death?
        DumpQueryOfDeath();
#endif  // _DEBUG
        return false;
      }
    } else {
      LOG(ERROR) << "EnsureSession failed: " << server_status_;
      return false;
    }
  }

  PushHistory(*input, *output);
  return true;
}

void Session::EnableCascadingWindow(const bool enable) {
  if (preferences_.get() == NULL) {
    preferences_.reset(new config::Config);
  }
  preferences_->set_use_cascading_window(enable);
}

void Session::set_timeout(int timeout) {
  timeout_ = timeout;
}

void Session::set_restricted(bool restricted) {
  server_launcher_->set_restricted(restricted);
}

void Session::set_server_program(const string &program_path) {
  server_launcher_->set_server_program(program_path);
}

bool Session::CreateSession() {
  id_ = 0;
  commands::Input input;
  input.set_type(commands::Input::CREATE_SESSION);

  commands::Output output;
  if (!CheckVersionOrRestartServerInternal(input, &output)) {
    LOG(ERROR) << "CheckVersionOrRestartServer() failed";
    return false;
  }

  if (output.error_code() != commands::Output::SESSION_SUCCESS) {
    LOG(ERROR) << "Server returns an error";
    server_status_ = SERVER_INVALID_SESSION;
    return false;
  }

  id_ = output.id();
  return true;
}

bool Session::DeleteSession() {
  // No need to delete session
  if (id_ == 0) {
    return true;
  }

  commands::Input input;
  InitInput(&input);
  input.set_type(commands::Input::DELETE_SESSION);

  commands::Output output;
  if (!Call(input, &output)) {
    LOG(ERROR) << "DeleteSession failed";
    return false;
  }
  id_ = 0;
  return true;
}

bool Session::GetConfig(config::Config *config) {
  commands::Input input;
  InitInput(&input);
  input.set_type(commands::Input::GET_CONFIG);

  commands::Output output;
  if (!Call(input, &output)) {
    return false;
  }

  if (!output.has_config()) {
    return false;
  }

  config->Clear();
  config->CopyFrom(output.config());
  return true;
}

bool Session::SetConfig(const config::Config &config) {
  commands::Input input;
  InitInput(&input);
  input.set_type(commands::Input::SET_CONFIG);
  input.mutable_config()->CopyFrom(config);

  commands::Output output;
  if (!Call(input, &output)) {
    return false;
  }

  return true;
}

bool Session::ClearUserHistory() {
  return CallCommand(commands::Input::CLEAR_USER_HISTORY);
}

bool Session::ClearUserPrediction() {
  return CallCommand(commands::Input::CLEAR_USER_PREDICTION);
}

bool Session::ClearUnusedUserPrediction() {
  return CallCommand(commands::Input::CLEAR_UNUSED_USER_PREDICTION);
}

bool Session::Shutdown() {
  CallCommand(commands::Input::SHUTDOWN);
  if (!server_launcher_->WaitServer(server_process_id_)) {
    LOG(ERROR) << "Cannot shutdown the server";
    return false;
  }
  return true;
}

bool Session::SyncData() {
  return CallCommand(commands::Input::SYNC_DATA);
}

bool Session::Reload() {
  return CallCommand(commands::Input::RELOAD);
}

bool Session::Cleanup() {
  return CallCommand(commands::Input::CLEANUP);
}

bool Session::NoOperation() {
  return CallCommand(commands::Input::NO_OPERATION);
}

// PingServer ignores all server status
bool Session::PingServer() const {
  commands::Input input;
  commands::Output output;

  InitInput(&input);
  input.set_type(commands::Input::NO_OPERATION);

 // Call IPC
  scoped_ptr<IPCClientInterface> client(
      client_factory_->NewClient(kServerAddress,
                                 server_launcher_->server_program()));

  if (client.get() == NULL) {
    LOG(ERROR) << "Cannot make client object";
    return false;
  }

  if (!client->Connected()) {
    LOG(ERROR) << "Connection failure to " << kServerAddress;
    return false;
  }

  // Serialize
  string request;
  input.SerializeToString(&request);

  size_t size = kResultBufferSize;
  if (!client->Call(request.data(), request.size(),
                    result_.get(), &size, timeout_)) {
    LOG(ERROR) << "IPCClient::Call failed: " << client->GetLastIPCError();
    return false;
  }

  return true;
}

bool Session::CallCommand(commands::Input::CommandType type) {
  commands::Input input;
  InitInput(&input);
  input.set_type(type);
  commands::Output output;
  return Call(input, &output);
}

bool Session::CallAndCheckVersion(const commands::Input &input,
                                  commands::Output *output) {
  if (!Call(input, output)) {
    if (server_protocol_version_ != IPC_PROTOCOL_VERSION) {
      LOG(ERROR) << "version mismatch: "
                 << server_protocol_version_ << " "
                 << static_cast<int>(IPC_PROTOCOL_VERSION);
      server_status_ = SERVER_VERSION_MISMATCH;
    }
    return false;
  }

  return true;
}

bool Session::Call(const commands::Input &input,
                   commands::Output *output) {
  VLOG(2) << "commands::Input: " << endl << input.DebugString();

  // don't repeat Call() if the status is either
  // SERVER_FATAL, SERVER_TIMEOUT, or SERVER_BROKEN_MESSAGE
  if (server_status_ >= SERVER_TIMEOUT) {
    LOG(ERROR) << "Don't repat the same status: " << server_status_;
    return false;
  }

  // Serialize
  string request;
  input.SerializeToString(&request);

  // Call IPC
  scoped_ptr<IPCClientInterface> client(
      client_factory_->NewClient(kServerAddress,
                                 server_launcher_->server_program()));

  // set client protocol version.
  // When an error occurs inside Connected() function,
  // the server_protocol_version_ may be set to
  // the default value defined in .proto file.
  // This caused an mis-version-detection.
  // To avoid such situation, we set the client protocol version
  // before calling IPC request.
  server_protocol_version_ = IPC_PROTOCOL_VERSION;
  server_product_version_ = Version::GetMozcVersion();
  server_process_id_ = 0;

  if (client.get() == NULL) {
    LOG(ERROR) << "Cannot make client object";
    server_status_ = SERVER_FATAL;
    return false;
  }

  if (!client->Connected()) {
    LOG(ERROR) << "Connection failure to " << kServerAddress;
    // if the status is not SERVER_UNKNOWN, it means that
    // the server WAS working as correctly.
    if (server_status_ != SERVER_UNKNOWN) {
      server_status_ = SERVER_SHUTDOWN;
    }
    return false;
  }

  server_protocol_version_ = client->GetServerProtocolVersion();
  server_product_version_ = client->GetServerProductVersion();
  server_process_id_ = client->GetServerProcessId();

  if (server_protocol_version_ != IPC_PROTOCOL_VERSION) {
    LOG(ERROR) << "Server version mismatch. skipped to update the status here";
    return false;
  }

  // Drop DebugString() as it raises segmentation fault.
  // http://b/2126375
  // TODO(taku): Investigate the error in detail.
  size_t size = kResultBufferSize;
  if (!client->Call(request.data(), request.size(),
                    result_.get(), &size, timeout_)) {
    LOG(ERROR) << "Call failure";
    //               << input.DebugString();
    if (client->GetLastIPCError() == IPC_TIMEOUT_ERROR) {
      server_status_ = SERVER_TIMEOUT;
    } else {
      // server crash
      server_status_ = SERVER_SHUTDOWN;
    }
    return false;
  }

  if (!output->ParseFromArray(result_.get(), size)) {
    LOG(ERROR) << "Parse failure of the result of the request:";
    //               << input.DebugString();
    server_status_ = SERVER_BROKEN_MESSAGE;
    return false;
  }

  DCHECK(server_status_ == SERVER_OK ||
         server_status_ == SERVER_INVALID_SESSION ||
         server_status_ == SERVER_SHUTDOWN ||
         server_status_ == SERVER_UNKNOWN /* during StartServer() */)
             << " " << server_status_;

  VLOG(2) << "commands::Output: " << endl << output->DebugString();

  return true;
}

bool Session::StartServer() {
  if (server_launcher_.get() != NULL) {
    return server_launcher_->StartServer(this);
  }
  return true;
}


void Session::OnFatal(ServerLauncherInterface::ServerErrorType type) {
  if (server_launcher_.get() != NULL) {
    server_launcher_->OnFatal(type);
  }
}

void Session::InitInput(commands::Input *input) const {
  input->set_id(id_);
  if (preferences_.get() != NULL) {
    input->mutable_config()->CopyFrom(*preferences_);
  }
}

bool Session::CheckVersionOrRestartServerInternal(
    const commands::Input &input, commands::Output *output) {
  for (int trial = 0; trial < 2; ++trial) {
    const bool call_result = Call(input, output);

    if (!call_result && server_protocol_version_ > IPC_PROTOCOL_VERSION) {
      LOG(ERROR) << "Server version is newer than client version.";
      server_status_ = SERVER_VERSION_MISMATCH;
      return false;
    }

    const bool version_upgraded =
        Version::CompareVersion(server_product_version_,
                                Version::GetMozcVersion());

    // if the server version is older than client version or
    // protocol version is updated, force to reboot the server.
    // if the version is even unchanged after the reboot, goes to
    // SERVER_VERSION_MISMATCH state, which brings the client into
    // SERVER_FATAL state finally.
    if ((call_result && version_upgraded) ||
        (!call_result && server_protocol_version_ < IPC_PROTOCOL_VERSION)) {
      LOG(WARNING) << "Version Mismatch: "
                   << server_product_version_ << " "
                   << Version::GetMozcVersion()
                   << " "
                   << server_protocol_version_ << " "
                   << static_cast<int>(IPC_PROTOCOL_VERSION)
                   << " " << trial;
      if (trial > 0) {
        LOG(ERROR) << "Server version mismatch even after server reboot";
        server_status_ = SERVER_BROKEN_MESSAGE;
        return false;
      }

      bool shutdown_result = true;
      if (call_result && version_upgraded) {
        // use shutdown command if the protocol version is compatible
        shutdown_result = Shutdown();
        if (!shutdown_result) {
          LOG(ERROR) << "Shutdown command failed";
        }
      }

      // force to terminate the process if protocol version is not compatible
      if (!shutdown_result ||
          (!call_result && server_protocol_version_ < IPC_PROTOCOL_VERSION)) {
        if (!server_launcher_->ForceTerminateServer(kServerAddress)) {
          LOG(ERROR) << "ForceTerminateProcess failed";
          server_status_ = SERVER_BROKEN_MESSAGE;
          return false;
        }

        if (!server_launcher_->WaitServer(server_process_id_)) {
          LOG(ERROR) << "Cannot terminate server process";
        }
      }

      server_status_ = SERVER_UNKNOWN;
      if (!EnsureConnection()) {
        server_status_ = SERVER_VERSION_MISMATCH;
        LOG(ERROR) << "Ensure Connecion failed";
        return false;
      }

      continue;
    }

    if (!call_result) {
      LOG(ERROR) << "Call() failed";
      return false;
    }

    return true;
  }

  return false;
}

void Session::Reset() {
  server_status_ = SERVER_UNKNOWN;
  server_protocol_version_ = 0;
  server_process_id_ = 0;
}

bool Session::IsAbortKey(const commands::KeyEvent &key) {
  return key.has_special_key() &&
      key.special_key() == commands::KeyEvent::F12 &&
      key.modifier_keys().size() == 2 &&
      ((key.modifier_keys(0) == commands::KeyEvent::CTRL &&
        key.modifier_keys(1) == commands::KeyEvent::ALT) ||
       (key.modifier_keys(0) == commands::KeyEvent::ALT &&
        key.modifier_keys(1) == commands::KeyEvent::CTRL));
}

bool Session::LaunchTool(const string &mode, const string &extra_arg) {
#ifdef OS_LINUX
  // TODO(taku): define it in const.h
  const char kMozcTool[] = "mozc_tool";
#endif  // OS_LINUX

  // Don't execute any child process if the parent process is not
  // in proper runlevel.
  if (!IsValidRunLevel()) {
    return false;
  }

  // Validate |mode|.
  // TODO(taku): better to validate the parameter more carefully.
  const size_t kModeMaxSize = 32;
  if (mode.empty() || mode.size() >= kModeMaxSize) {
    LOG(ERROR) << "Invalid mode: " << mode;
    return false;
  }

  if (mode == "administration_dialog") {
#ifdef OS_WINDOWS
    const string path = mozc::Util::JoinPath
        (mozc::Util::GetServerDirectory(), kMozcTool);
    wstring wpath;
    Util::UTF8ToWide(path.c_str(), &wpath);
    wpath = L"\"" + wpath + L"\"";
    // Run administration dialog with UAC.
    // AFAIK, ShellExecute is only the way to launch process with
    // UAC protection.
    // No COM operations are executed as ShellExecute is only
    // used for launching an UAC-process.
    //
    // In Windows XP, cannot use "runas", instead, administration
    // dialog is launched with normal process with "open"
    // http://b/2415191
    const int result =
        reinterpret_cast<int>(::ShellExecute(0,
                                             mozc::Util::IsVistaOrLater() ?
                                             L"runas" : L"open",
                                             wpath.c_str(),
                                             L"--mode=administration_dialog",
                                             mozc::Util::GetSystemDir(),
                                             SW_SHOW));
    if (result <= 32) {
      LOG(ERROR) << "::ShellExecute failed: " << result;
      return false;
    }
#endif  // OS_WINDOWS

    return false;
  }

#if defined(OS_WINDOWS) || defined(OS_LINUX)
  string arg = "--mode=" + mode;
  if (!extra_arg.empty()) {
    arg += " ";
    arg += extra_arg;
  }
  if (!mozc::Process::SpawnMozcProcess(kMozcTool, arg)) {
    LOG(ERROR) << "Cannot execute: " << kMozcTool << " " << arg;
    return false;
  }
#endif  // OS_WINDOWS || OS_LINUX

  // TODO(taku): move MacProcess inside SpawnMozcProcess.
  // TODO(taku): support extra_arg.
#ifdef OS_MACOSX
  if (!MacProcess::LaunchMozcTool(mode)) {
    LOG(ERROR) << "Cannot execute: " << mode;
    return false;
  }
#endif  // OS_MACOSX

  return true;
}

bool Session::OpenBrowser(const string &url) {
  if (!IsValidRunLevel()) {
    return false;
  }

  if (!Process::OpenBrowser(url)) {
    LOG(ERROR) << "Process::OpenBrowser failed.";
    return false;
  }

  return true;
}

}  // namespace client
}  // namespace mozc
