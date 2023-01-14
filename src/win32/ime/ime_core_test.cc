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

// clang-format off
#include <windows.h>
#include <ime.h>
#include <msctf.h>
// clang-format on

#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "base/version.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "ipc/ipc_mock.h"
#include "protocol/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"
#include "win32/ime/ime_core.h"

namespace mozc {
namespace win32 {
namespace {
class TestServerLauncher : public client::ServerLauncherInterface {
 public:
  explicit TestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  virtual bool StartServer(client::ClientInterface *client) {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  virtual bool ForceTerminateServer(const std::string &name) { return true; }

  virtual bool WaitServer(uint32 pid) { return true; }

  virtual void OnFatal(ServerLauncherInterface::ServerErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
    error_map_[static_cast<int>(type)]++;
  }

  int error_count(ServerLauncherInterface::ServerErrorType type) {
    return error_map_[static_cast<int>(type)];
  }

  bool start_server_called() const { return start_server_called_; }

  void set_start_server_called(bool start_server_called) {
    start_server_called_ = start_server_called;
  }

  virtual void set_restricted(bool restricted) {}

  virtual void set_suppress_error_dialog(bool suppress) {}

  virtual void set_server_program(const std::string &server_path) {}

  virtual const std::string &server_program() const {
    static const std::string path;
    return path;
  }

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  void set_server_protocol_version(uint32 server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }

  uint32 set_server_protocol_version() const {
    return server_protocol_version_;
  }

  void set_mock_after_start_server(const commands::Output &mock_output) {
    mock_output.SerializeToString(&response_);
  }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  uint32 server_protocol_version_;
  std::string response_;
  std::map<int, int> error_map_;
};

class MockClient : public client::Client {
 public:
  MockClient() : launcher_(nullptr) {}
  MockClient(const MockClient &) = delete;
  MockClient &operator=(const MockClient &) = delete;
  explicit MockClient(const commands::Output &mock_response)
      : launcher_(nullptr) {
    client_factory_.SetConnection(true);
    client_factory_.SetResult(true);
    client_factory_.SetServerProductVersion(Version::GetMozcVersion());
    client_factory_.SetMockResponse(mock_response.SerializeAsString());
    SetIPCClientFactory(&client_factory_);

    // |launcher| will be deleted by the |client|
    launcher_ = new TestServerLauncher(&client_factory_);
    SetServerLauncher(launcher_);
    launcher_->set_start_server_result(true);
  }

  bool GetGeneratedRequest(mozc::commands::Input *input) const {
    return input->ParseFromString(client_factory_.GetGeneratedRequest());
  }

  bool start_server_called() { return launcher_->start_server_called(); }

 private:
  IPCClientFactoryMock client_factory_;
  TestServerLauncher *launcher_;
};
}  // namespace

TEST(ImeCoreTest, OpenIME) {
  commands::Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::FULL_KATAKANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::FULL_KATAKANA);

  MockClient mock_client(mock_output);
  constexpr DWORD kFullKatakana = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE |
                                  IME_CMODE_ROMAN | IME_CMODE_KATAKANA;
  EXPECT_TRUE(ImeCore::OpenIME(&mock_client, kFullKatakana));
  {
    commands::Input actual_input;
    ASSERT_TRUE(mock_client.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_COMMAND, actual_input.type());
    ASSERT_TRUE(actual_input.has_command());
    ASSERT_TRUE(actual_input.command().has_type());
    EXPECT_EQ(commands::SessionCommand::TURN_ON_IME,
              actual_input.command().type());
    ASSERT_TRUE(actual_input.command().has_composition_mode());
    EXPECT_EQ(commands::FULL_KATAKANA,
              actual_input.command().composition_mode());
  }
}

TEST(ImeCoreTest, CloseIME) {
  commands::Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::DIRECT);
  mock_output.mutable_status()->set_activated(false);
  mock_output.mutable_status()->set_mode(commands::FULL_KATAKANA);

  MockClient mock_client(mock_output);

  constexpr DWORD kFullKatakana = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE |
                                  IME_CMODE_ROMAN | IME_CMODE_KATAKANA;

  commands::Output output;
  EXPECT_TRUE(ImeCore::CloseIME(&mock_client, kFullKatakana, &output));
  {
    commands::Input actual_input;
    ASSERT_TRUE(mock_client.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_COMMAND, actual_input.type());
    ASSERT_TRUE(actual_input.has_command());
    ASSERT_TRUE(actual_input.command().has_type());
    EXPECT_EQ(commands::SessionCommand::TURN_OFF_IME,
              actual_input.command().type());
    ASSERT_TRUE(actual_input.command().has_composition_mode());
    EXPECT_EQ(commands::FULL_KATAKANA,
              actual_input.command().composition_mode());
  }
}

TEST(ImeCoreTest, GetSupportableConversionMode) {
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE,
            ImeCore::GetSupportableConversionMode(IME_CMODE_NATIVE |
                                                  IME_CMODE_FULLSHAPE));

  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
            ImeCore::GetSupportableConversionMode(
                IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN));
}

TEST(ImeCoreTest, GetSupportableConversionMode_Issue2914115) {
  // http://b/2914115
  // In some environment, its initial conversion mode is IME_CMODE_NATIVE
  // (only), which is not the one of the expected conversion modes for Mozc.
  // Assume Full-width Hiragana in this case.
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
            ImeCore::GetSupportableConversionMode(IME_CMODE_NATIVE));
}

TEST(ImeCoreTest, GetSupportableSentenceMode) {
  // Always returns IME_SMODE_PHRASEPREDICT.
  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_NONE));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_PLAURALCLAUSE));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_SINGLECONVERT));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_AUTOMATIC));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_PHRASEPREDICT));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_CONVERSATION));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_RESERVED));

  EXPECT_EQ(IME_SMODE_PHRASEPREDICT,
            ImeCore::GetSupportableSentenceMode(MAXDWORD));
}

TEST(ImeCoreTest, GetSupportableSentenceMode_Issue2955175) {
  // b/2955175
  // If Mozc leaves the sentence mode IME_SMODE_NONE and switch to MS-IME in
  // Sylpheed, MS-IME cannot convert composition string because it's in
  // Muhenkan-mode.
  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_NONE));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_PLAURALCLAUSE));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_SINGLECONVERT));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_AUTOMATIC));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_PHRASEPREDICT));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_CONVERSATION));

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(IME_SMODE_RESERVED));

  EXPECT_NE(IME_SMODE_NONE, ImeCore::GetSupportableSentenceMode(MAXDWORD));
}

namespace {
// constants for unit tests
constexpr DWORD kHiragana = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
constexpr DWORD kHalfAlpha = IME_CMODE_ALPHANUMERIC;

// Mozc uses only one candidate form.  This is why |kCandidateFormIndex| is
// always to be 1.
constexpr DWORD kCandidateFormIndex = 1;

// Currently, Mozc always set 0 (L'\0') to the wparam of the
// WM_IME_COMPOSITION.
// TODO(yukawa): Support wparam of the WM_IME_COMPOSITION.
const wchar_t kLastUpdatedCharacter = L'\0';

constexpr DWORD kCompositionUpdateFlag =
    GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
    GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART;
static_assert(kCompositionUpdateFlag == 0x1bf, "Must be 0x1bf");

constexpr DWORD kCompositionResultFlag =
    GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE | GCS_RESULTSTR | GCS_RESULTCLAUSE;
static_assert(kCompositionResultFlag == 0x1e00, "Must be 0x1e00");

constexpr DWORD kCompositionResultAndUpdateFlag =
    kCompositionResultFlag | kCompositionUpdateFlag;
static_assert(kCompositionResultAndUpdateFlag == 0x1fbf, "Must be 0x1fbf");

const UIMessage kMsgMozcUIUpdate(WM_IME_NOTIFY, IMN_PRIVATE, kNotifyUpdateUI);
const UIMessage kMsgSetConversionMode(WM_IME_NOTIFY, IMN_SETCONVERSIONMODE, 0);
const UIMessage kMsgStartComposition(WM_IME_STARTCOMPOSITION, 0, 0);
const UIMessage kMsgCompositionUpdate(WM_IME_COMPOSITION, kLastUpdatedCharacter,
                                      kCompositionUpdateFlag);
const UIMessage kMsgCompositionResult(WM_IME_COMPOSITION, kLastUpdatedCharacter,
                                      kCompositionResultFlag);
const UIMessage kMsgCompositionResultAndUpdate(WM_IME_COMPOSITION,
                                               kLastUpdatedCharacter,
                                               kCompositionResultAndUpdateFlag);
const UIMessage kMsgEndComposition(WM_IME_ENDCOMPOSITION, 0, 0);

const UIMessage kMsgOpenCandidate(WM_IME_NOTIFY, IMN_OPENCANDIDATE,
                                  kCandidateFormIndex);
const UIMessage kMsgChangeCandidate(WM_IME_NOTIFY, IMN_CHANGECANDIDATE,
                                    kCandidateFormIndex);
const UIMessage kMsgCloseCandidate(WM_IME_NOTIFY, IMN_CLOSECANDIDATE,
                                   kCandidateFormIndex);

#define EXPECT_UI_MSG(expected_message, expected_wparam, expected_lparam, \
                      actual_ui_msg)                                      \
  do {                                                                    \
    EXPECT_EQ((expected_message), (actual_ui_msg).message());             \
    EXPECT_EQ((expected_wparam), (actual_ui_msg).wparam());               \
    EXPECT_EQ((expected_lparam), (actual_ui_msg).lparam());               \
  } while (false)

#define EXPECT_UI_UPDATE_MSG(actual_msg) \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_PRIVATE, kNotifyUpdateUI, (actual_msg))
#define EXPECT_SET_OPEN_STATUS_MSG(actual_msg) \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0, (actual_msg))
#define EXPECT_SET_CONVERSION_MODE_MSG(actual_msg) \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_SETCONVERSIONMODE, 0, (actual_msg))
#define EXPECT_STARTCOMPOSITION_MSG(actual_msg) \
  EXPECT_UI_MSG(WM_IME_STARTCOMPOSITION, 0, 0, (actual_msg))
#define EXPECT_COMPOSITION_MSG(expected_flag, actual_msg)                   \
  EXPECT_UI_MSG(WM_IME_COMPOSITION, kLastUpdatedCharacter, (expected_flag), \
                (actual_msg))
#define EXPECT_ENDCOMPOSITION_MSG(actual_msg) \
  EXPECT_UI_MSG(WM_IME_ENDCOMPOSITION, 0, 0, (actual_msg))
#define EXPECT_OPENCANDIDATE_MSG(actual_msg)                           \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_OPENCANDIDATE, kCandidateFormIndex, \
                (actual_msg))
#define EXPECT_CHANGECANDIDATE_MSG(actual_msg)                           \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, kCandidateFormIndex, \
                (actual_msg))
#define EXPECT_CLOSECANDIDATE_MSG(actual_msg)                           \
  EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, kCandidateFormIndex, \
                (actual_msg))
}  // namespace

// Check UI message order for
//   "Hankaku/Zenkaku" -> "(Shift +)G" -> "Hankaku/Zenkaku"
TEST(ImeCoreTest, TemporalConversionModeMessageOrderTest) {
  // "Hankaku/Zenkaku"
  {
    std::vector<UIMessage> composition_messages;
    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, false,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "(Shift +)G"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHalfAlpha, &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[1]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "Hankaku/Zenkaku"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResult);
    composition_messages.push_back(kMsgEndComposition);
    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHalfAlpha, false, kHiragana, &sorted_messages);

    EXPECT_EQ(5, sorted_messages.size());
    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionResultFlag, sorted_messages[1]);
    EXPECT_ENDCOMPOSITION_MSG(sorted_messages[2]);
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[3]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[4]);
  }
}

// Check UI message order for
//   "Hankaku/Zenkaku" -> "(Shift +)G" -> "o" -> "Enter" -> "Hankaku/Zenkaku"
TEST(ImeCoreTest, CompositionMessageOrderTest) {
  // "Hankaku/Zenkaku"
  {
    std::vector<UIMessage> composition_messages;
    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, false,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "(Shift +)G"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHalfAlpha, &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[1]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "o"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHalfAlpha, true, kHalfAlpha, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "Enter"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResult);
    composition_messages.push_back(kMsgEndComposition);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHalfAlpha, true, kHiragana, &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionResultFlag, sorted_messages[1]);
    EXPECT_ENDCOMPOSITION_MSG(sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "Hankaku/Zenkaku"
  {
    std::vector<UIMessage> composition_messages;
    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, false, kHiragana, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }
}

// Check UI message order for
//   "Hankaku/Zenkaku" -> "a" -> "Space" -> "Space" -> "Space" -> "A"
TEST(ImeCoreTest, CandidateMessageOrderTest) {
  // "Hankaku/Zenkaku"
  {
    std::vector<UIMessage> composition_messages;
    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, false,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "a"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "Space"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "Space"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgOpenCandidate);

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_OPENCANDIDATE_MSG(sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "Space"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    std::vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgChangeCandidate);

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_CHANGECANDIDATE_MSG(sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "i"
  {
    std::vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResultAndUpdate);

    std::vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgCloseCandidate);

    std::vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages, candidate_messages, true,
                             kHiragana, true, kHiragana, &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());
    // IMN_CLOSECANDIDATE must prior to any composition message!
    EXPECT_CLOSECANDIDATE_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionResultAndUpdateFlag, sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }
}
}  // namespace win32
}  // namespace mozc
