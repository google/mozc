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

// A class handling the IPC connection for the session b/w server and clients.

#include "client/client.h"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/const.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/process.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/version.h"
#include "base/vlog.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/key_info_util.h"

#ifdef _WIN32
#include <windows.h>

#include "base/win32/wide_char.h"
#include "base/win32/win_util.h"
#else  // _WIN32
#include <unistd.h>
#endif  // _WIN32

#ifdef __APPLE__
#include "base/mac/mac_process.h"
#endif  // __APPLE__

namespace mozc {
namespace client {

namespace {
constexpr char kServerAddress[] = "session";  // name for the IPC connection.
constexpr int kResultBufferSize = 8192 * 32;  // size of IPC buffer
constexpr size_t kMaxPlayBackSize = 512;      // size of maximum history

#ifdef DEBUG
constexpr absl::Duration kDefaultTimeout =
    absl::Seconds(100);  // 100 sec for dbg
#else                    // DEBUG
constexpr absl::Duration kDefaultTimeout = absl::Seconds(30);  // 30 sec for opt
#endif                   // DEBUG

// Delete Session is called inside the Destructor of Client class.
// To prevent from an application being stalled at the close time,
// we change the timeout of DeleteSession shorter.
// This timeout is only applied in the DeleteSessions command
// called from Destructor. When an application calls DeleteSession
// explicitly, the default timeout is used.
constexpr absl::Duration kDeleteSessionOnDestructorTimeout = absl::Seconds(1);
}  // namespace

Client::Client()
    : id_(0),
      server_launcher_(new ServerLauncher),
      timeout_(kDefaultTimeout),
      server_status_(SERVER_UNKNOWN),
      server_protocol_version_(0),
      server_process_id_(0),
      last_mode_(commands::DIRECT) {
  response_.reserve(kResultBufferSize);
  client_factory_ = IPCClientFactory::GetIPCClientFactory();

  // Initialize direct_mode_keys_
  direct_mode_keys_ = KeyInfoUtil::ExtractSortedDirectModeKeys(
      *config::ConfigHandler::GetSharedConfig());

#ifdef MOZC_USE_SVS_JAPANESE
  InitRequestForSvsJapanese(true);
#endif  // MOZC_USE_SVS_JAPANESE
}

Client::~Client() {
  set_timeout(kDeleteSessionOnDestructorTimeout);
  DeleteSession();
}

void Client::InitRequestForSvsJapanese(bool use_svs) {
  request_ = std::make_unique<commands::Request>();

  commands::DecoderExperimentParams params;
  uint32_t variation_types = params.variation_character_types();
  if (use_svs) {
    variation_types |= commands::DecoderExperimentParams::SVS_JAPANESE;
  } else {
    variation_types &= ~commands::DecoderExperimentParams::SVS_JAPANESE;
  }
  request_->mutable_decoder_experiment_params()->set_variation_character_types(
      variation_types);
}

bool Client::EnsureConnection() {
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
#ifdef DEBUG
      OnFatal(ServerLauncherInterface::SERVER_SHUTDOWN);
#endif  // DEBUG
      // don't break here as SERVER_SHUTDOWN and SERVER_UNKNOWN
      // have basically the same treatment.
      ABSL_FALLTHROUGH_INTENDED;
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

bool Client::EnsureSession() {
  if (!EnsureConnection()) {
    return false;
  }

  if (server_status_ == SERVER_OK) {
    return true;
  }
  DCHECK_EQ(server_status_, SERVER_INVALID_SESSION);

  if (!CreateSession()) {
    LOG(ERROR) << "CreateSession failed";
    // call EnsureConnection to display error message
    EnsureConnection();
    return false;
  }

  // Call SET_REQUEST if request_ is not nullptr.
  if (request_) {
    commands::Input input;
    input.set_id(id_);
    input.set_type(commands::Input::SET_REQUEST);
    *input.mutable_request() = *request_;
    commands::Output output;
    Call(input, &output);
  }

  server_status_ = SERVER_OK;
  return true;
}

void Client::DumpQueryOfDeath() {
  LOG(ERROR) << "The playback history looks like a query of death";
  constexpr char kFilename[] = "query_of_death.log";
  constexpr char kLabel[] = "Query of Death";
  DumpHistorySnapshot(kFilename, kLabel);
  ResetHistory();
}

void Client::DumpHistorySnapshot(const absl::string_view filename,
                                 const absl::string_view label) const {
  const std::string snapshot_file =
      FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), filename);
  // open with append mode
  OutputFileStream output(snapshot_file, std::ios::app);

  output << "---- Start history snapshot for " << label << std::endl;
  output << "Version " << Version::GetMozcVersion() << std::endl;
  for (size_t i = 0; i < history_inputs_.size(); ++i) {
    output << absl::StrCat(history_inputs_[i]);
  }
  output << "---- End history snapshot for " << label << std::endl;
}

void Client::PlaybackHistory() {
  if (history_inputs_.size() >= kMaxPlayBackSize) {
    ResetHistory();
    return;
  }

  commands::Output output;
  MOZC_VLOG(1) << "Playback history: size=" << history_inputs_.size();
  for (size_t i = 0; i < history_inputs_.size(); ++i) {
    history_inputs_[i].set_id(id_);
    if (!Call(history_inputs_[i], &output)) {
      LOG(ERROR) << "playback history failed: " << history_inputs_[i];
      break;
    }
  }
}

void Client::PushHistory(const commands::Input &input,
                         const commands::Output &output) {
  if (!output.has_consumed() || !output.consumed()) {
    // Do not remember unconsumed input.
    return;
  }

  // Update mode
  if (output.has_mode()) {
    last_mode_ = output.mode();
  }

  // don't insert a new input when history_inputs_.size()
  // reaches to the maximum size. This prevents DOS attack.
  if (history_inputs_.size() < kMaxPlayBackSize) {
    history_inputs_.push_back(input);
  }

  // found context boundary.
  // don't regard the empty output (output without preedit) as the context
  // boundary, as the IMEOn command make the empty output.
  if (input.type() == commands::Input::SEND_KEY && output.has_result()) {
    ResetHistory();
  }
}

// Clear the history and push IMEOn command for initialize session.
void Client::ResetHistory() {
  history_inputs_.clear();
#if defined(__APPLE__)
  // On Mac, we should send ON key at the first of each input session
  // excepting the very first session, because when the session is restored,
  // its state is direct. On the first session, users should send ON key
  // by themselves.
  // On Windows, this is not required because now we can send IME On/Off
  // state with the key event. See b/8601275
  // Note that we are assuming that ResetHistory is called only when the
  // client is ON.
  // TODO(toshiyuki): Make sure that this assuming is reasonable or not.
  if (last_mode_ != commands::DIRECT) {
    commands::Input input;
    input.set_type(commands::Input::SEND_KEY);
    input.mutable_key()->set_special_key(commands::KeyEvent::ON);
    input.mutable_key()->set_mode(last_mode_);
    history_inputs_.push_back(input);
  }
#endif  // __APPLE__
}

void Client::GetHistoryInputs(std::vector<commands::Input> *result) const {
  result->clear();
  for (size_t i = 0; i < history_inputs_.size(); ++i) {
    result->push_back(history_inputs_[i]);
  }
}

bool Client::SendKeyWithContext(const commands::KeyEvent &key,
                                const commands::Context &context,
                                commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  *input.mutable_key() = key;
  // If the pointer of |context| is not the default_instance, update the data.
  if (&context != &commands::Context::default_instance()) {
    *input.mutable_context() = context;
  }
  return EnsureCallCommand(&input, output);
}

bool Client::TestSendKeyWithContext(const commands::KeyEvent &key,
                                    const commands::Context &context,
                                    commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  // If the pointer of |context| is not the default_instance, update the data.
  if (&context != &commands::Context::default_instance()) {
    *input.mutable_context() = context;
  }
  *input.mutable_key() = key;
  return EnsureCallCommand(&input, output);
}

bool Client::SendCommandWithContext(const commands::SessionCommand &command,
                                    const commands::Context &context,
                                    commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  *input.mutable_command() = command;
  // If the pointer of |context| is not the default_instance, update the data.
  if (&context != &commands::Context::default_instance()) {
    *input.mutable_context() = context;
  }
  return EnsureCallCommand(&input, output);
}

bool Client::CheckVersionOrRestartServer() {
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

bool Client::EnsureCallCommand(commands::Input *input,
                               commands::Output *output) {
  if (!EnsureSession()) {
    LOG(ERROR) << "EnsureSession failed";
    return false;
  }

  InitInput(input);
  output->set_id(0);

  if (!CallAndCheckVersion(*input, output)) {  // server is not running
    LOG(ERROR) << "Call command failed";
  } else if (output->id() != input->id()) {  // invalid ID
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
#ifdef DEBUG
      // The debug binary dumps query of death at the first trial.
      history_inputs_.push_back(*input);
      DumpQueryOfDeath();
#endif  // DEBUG
      // second trial
      if (!CallAndCheckVersion(*input, output)) {
#ifndef DEBUG
        // if second trial failed, record the input
        history_inputs_.push_back(*input);
        // Opt or release binaries refrain from dumping query of death
        // at the first trial, but dumps it at the second trial.
        //
        // TODO(komatsu, taku): Should release binary dump query of death?
        DumpQueryOfDeath();
#endif  // DEBUG
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

void Client::EnableCascadingWindow(const bool enable) {
  if (preferences_ == nullptr) {
    preferences_ = std::make_unique<config::Config>();
  }
  preferences_->set_use_cascading_window(enable);
}

void Client::set_timeout(absl::Duration timeout) { timeout_ = timeout; }

void Client::set_restricted(bool restricted) {
  server_launcher_->set_restricted(restricted);
}

void Client::set_server_program(const absl::string_view program_path) {
  server_launcher_->set_server_program(program_path);
}

void Client::set_suppress_error_dialog(bool suppress) {
  server_launcher_->set_suppress_error_dialog(suppress);
}

void Client::set_client_capability(const commands::Capability &capability) {
  client_capability_ = capability;
}

bool Client::CreateSession() {
  id_ = 0;
  commands::Input input;
  input.set_type(commands::Input::CREATE_SESSION);

  *input.mutable_capability() = client_capability_;

  commands::ApplicationInfo *info = input.mutable_application_info();
  DCHECK(info);

#ifdef _WIN32
  info->set_process_id(static_cast<uint32_t>(::GetCurrentProcessId()));
  info->set_thread_id(static_cast<uint32_t>(::GetCurrentThreadId()));
#else   // _WIN32
  info->set_process_id(static_cast<uint32_t>(getpid()));
  info->set_thread_id(0);
#endif  // _WIN32

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

bool Client::DeleteSession() {
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

bool Client::IsDirectModeCommand(const commands::KeyEvent &key) const {
  return KeyInfoUtil::ContainsKey(direct_mode_keys_, key);
}

bool Client::GetConfig(config::Config *config) {
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
  *config = output.config();
  return true;
}

bool Client::SetConfig(const config::Config &config) {
  commands::Input input;
  InitInput(&input);
  input.set_type(commands::Input::SET_CONFIG);
  *input.mutable_config() = config;

  commands::Output output;
  if (!Call(input, &output)) {
    return false;
  }

  direct_mode_keys_ = KeyInfoUtil::ExtractSortedDirectModeKeys(config);
  return true;
}

bool Client::ClearUserHistory() {
  return CallCommand(commands::Input::CLEAR_USER_HISTORY);
}

bool Client::ClearUserPrediction() {
  return CallCommand(commands::Input::CLEAR_USER_PREDICTION);
}

bool Client::ClearUnusedUserPrediction() {
  return CallCommand(commands::Input::CLEAR_UNUSED_USER_PREDICTION);
}

bool Client::Shutdown() {
  CallCommand(commands::Input::SHUTDOWN);
  if (!server_launcher_->WaitServer(server_process_id_)) {
    LOG(ERROR) << "Cannot shutdown the server";
    return false;
  }
  return true;
}

bool Client::SyncData() { return CallCommand(commands::Input::SYNC_DATA); }

bool Client::Reload() { return CallCommand(commands::Input::RELOAD); }

bool Client::Cleanup() { return CallCommand(commands::Input::CLEANUP); }

bool Client::NoOperation() {
  return CallCommand(commands::Input::NO_OPERATION);
}

// PingServer ignores all server status
bool Client::PingServer() const {
  if (client_factory_ == nullptr) {
    return false;
  }

  commands::Input input;
  commands::Output output;

  InitInput(&input);
  input.set_type(commands::Input::NO_OPERATION);

  // Call IPC
  std::unique_ptr<IPCClientInterface> client(client_factory_->NewClient(
      kServerAddress, server_launcher_->server_program()));

  if (client == nullptr) {
    LOG(ERROR) << "Cannot make client object";
    return false;
  }

  if (!client->Connected()) {
    LOG(ERROR) << "Connection failure to " << kServerAddress;
    return false;
  }

  // Serialize
  std::string request;
  std::string response;
  input.SerializeToString(&request);

  if (!client->Call(request, &response, timeout_)) {
    LOG(ERROR) << "IPCClient::Call failed: " << client->GetLastIPCError();
    return false;
  }

  return true;
}

bool Client::CallCommand(commands::Input::CommandType type) {
  commands::Input input;
  InitInput(&input);
  input.set_type(type);
  commands::Output output;
  return Call(input, &output);
}

bool Client::CallAndCheckVersion(const commands::Input &input,
                                 commands::Output *output) {
  if (!Call(input, output)) {
    if (server_protocol_version_ != IPC_PROTOCOL_VERSION) {
      LOG(ERROR) << "version mismatch: " << server_protocol_version_ << " "
                 << static_cast<int>(IPC_PROTOCOL_VERSION);
      server_status_ = SERVER_VERSION_MISMATCH;
    }
    return false;
  }

  return true;
}

bool Client::Call(const commands::Input &input, commands::Output *output) {
  MOZC_VLOG(2) << "commands::Input: " << std::endl << input;

  // don't repeat Call() if the status is either
  // SERVER_FATAL, SERVER_TIMEOUT, or SERVER_BROKEN_MESSAGE
  if (server_status_ >= SERVER_TIMEOUT) {
    LOG(ERROR) << "Don't repat the same status: " << server_status_;
    return false;
  }

  if (client_factory_ == nullptr) {
    return false;
  }

  // Serialize
  std::string request;
  input.SerializeToString(&request);

  // Call IPC
  std::unique_ptr<IPCClientInterface> client(client_factory_->NewClient(
      kServerAddress, server_launcher_->server_program()));

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

  if (client == nullptr) {
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

  if (!client->Call(request, &response_, timeout_)) {
    LOG(ERROR) << "Call failure" << input.DebugString();
    if (client->GetLastIPCError() == IPC_TIMEOUT_ERROR) {
      server_status_ = SERVER_TIMEOUT;
    } else {
      // server crash
      server_status_ = SERVER_SHUTDOWN;
    }
    return false;
  }

  if (!output->ParseFromString(response_)) {
    LOG(ERROR) << "Parse failure of the result of the request:"
               << input.DebugString();
    server_status_ = SERVER_BROKEN_MESSAGE;
    return false;
  }

  DCHECK(server_status_ == SERVER_OK ||
         server_status_ == SERVER_INVALID_SESSION ||
         server_status_ == SERVER_SHUTDOWN ||
         server_status_ == SERVER_UNKNOWN /* during StartServer() */)
      << " " << server_status_;

  MOZC_VLOG(2) << "commands::Output: " << std::endl << *output;

  return true;
}

bool Client::StartServer() {
  if (server_launcher_ != nullptr) {
    return server_launcher_->StartServer(this);
  }
  return true;
}

void Client::OnFatal(ServerLauncherInterface::ServerErrorType type) {
  if (server_launcher_ != nullptr) {
    server_launcher_->OnFatal(type);
  }
}

void Client::InitInput(commands::Input *input) const {
  input->set_id(id_);
  if (preferences_ != nullptr) {
    *input->mutable_config() = *preferences_;
  }
}

bool Client::CheckVersionOrRestartServerInternal(const commands::Input &input,
                                                 commands::Output *output) {
  for (int trial = 0; trial < 2; ++trial) {
    const bool call_result = Call(input, output);

    if (!call_result && server_protocol_version_ > IPC_PROTOCOL_VERSION) {
      LOG(ERROR) << "Server version is newer than client version.";
      server_status_ = SERVER_VERSION_MISMATCH;
      return false;
    }

    const bool version_upgraded = Version::CompareVersion(
        server_product_version_, Version::GetMozcVersion());

    // if the server version is older than client version or
    // protocol version is updated, force to reboot the server.
    // if the version is even unchanged after the reboot, goes to
    // SERVER_VERSION_MISMATCH state, which brings the client into
    // SERVER_FATAL state finally.
    if ((call_result && version_upgraded) ||
        (!call_result && server_protocol_version_ < IPC_PROTOCOL_VERSION)) {
      LOG(WARNING) << "Version Mismatch: " << server_product_version_ << " "
                   << Version::GetMozcVersion() << " "
                   << server_protocol_version_ << " "
                   << static_cast<int>(IPC_PROTOCOL_VERSION) << " " << trial;
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
        LOG(ERROR) << "Ensure Connection failed";
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

void Client::Reset() {
  server_status_ = SERVER_UNKNOWN;
  server_protocol_version_ = 0;
  server_process_id_ = 0;
}

bool Client::TranslateProtoBufToMozcToolArg(const commands::Output &output,
                                            std::string *mode) {
  if (!output.has_launch_tool_mode() || mode == nullptr) {
    return false;
  }

  switch (output.launch_tool_mode()) {
    case commands::Output::CONFIG_DIALOG:
      mode->assign("config_dialog");
      break;
    case commands::Output::DICTIONARY_TOOL:
      mode->assign("dictionary_tool");
      break;
    case commands::Output::WORD_REGISTER_DIALOG:
      mode->assign("word_register_dialog");
      break;
    case commands::Output::NO_TOOL:
    default:
      // do nothing
      return false;
      break;
  }

  return true;
}

bool Client::LaunchToolWithProtoBuf(const commands::Output &output) {
  std::string mode;
  if (!TranslateProtoBufToMozcToolArg(output, &mode)) {
    return false;
  }

  // TODO(nona): extends output message to support extra argument.
  return LaunchTool(mode, "");
}

bool Client::LaunchTool(const std::string &mode,
                        const absl::string_view extra_arg) {
  // Don't execute any child process if the parent process is not
  // in proper runlevel.
  if (!IsValidRunLevel()) {
    return false;
  }

  // Validate |mode|.
  // TODO(taku): better to validate the parameter more carefully.
  constexpr size_t kModeMaxSize = 32;
  if (mode.empty() || mode.size() >= kModeMaxSize) {
    LOG(ERROR) << "Invalid mode: " << mode;
    return false;
  }

  if (mode == "administration_dialog") {
#ifdef _WIN32
    std::wstring wpath = L"\"";
    wpath += win32::Utf8ToWide(mozc::SystemUtil::GetToolPath());
    wpath += L"\"";
    // Run administration dialog with UAC.
    // AFAIK, ShellExecute is only the way to launch process with
    // UAC protection.
    // No COM operations are executed as ShellExecute is only
    // used for launching an UAC-process.
    //
    // In Windows XP, cannot use "runas", instead, administration
    // dialog is launched with normal process with "open"
    // http://b/2415191
    return WinUtil::ShellExecuteInSystemDir(L"runas", wpath.c_str(),
                                            L"--mode=administration_dialog");
#endif  // _WIN32

    return false;
  }

#if defined(_WIN32) || defined(__linux__)
  std::string arg = absl::StrCat("--mode=", mode);
  if (!extra_arg.empty()) {
    absl::StrAppend(&arg, " ", extra_arg);
  }
  if (!mozc::Process::SpawnMozcProcess(kMozcTool, arg)) {
    LOG(ERROR) << "Cannot execute: " << kMozcTool << " " << arg;
    return false;
  }
#endif  // _WIN32 || __linux__

  // TODO(taku): move MacProcess inside SpawnMozcProcess.
  // TODO(taku): support extra_arg.
#ifdef __APPLE__
  if (!MacProcess::LaunchMozcTool(mode)) {
    LOG(ERROR) << "Cannot execute: " << mode;
    return false;
  }
#endif  // __APPLE__

  return true;
}

bool Client::OpenBrowser(const std::string &url) {
  if (!IsValidRunLevel()) {
    return false;
  }

  if (!Process::OpenBrowser(url)) {
    LOG(ERROR) << "Process::OpenBrowser failed.";
    return false;
  }

  return true;
}

namespace {
class DefaultClientFactory : public ClientFactoryInterface {
 public:
  std::unique_ptr<ClientInterface> NewClient() override {
    return std::make_unique<Client>();
  }
};

ClientFactoryInterface *g_client_factory = nullptr;
}  // namespace

std::unique_ptr<ClientInterface> ClientFactory::NewClient() {
  if (g_client_factory == nullptr) {
    return Singleton<DefaultClientFactory>::get()->NewClient();
  } else {
    return g_client_factory->NewClient();
  }
}

void ClientFactory::SetClientFactory(ClientFactoryInterface *client_factory) {
  g_client_factory = client_factory;
}
}  // namespace client
}  // namespace mozc
