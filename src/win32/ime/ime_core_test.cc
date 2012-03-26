// Copyright 2010-2012, Google Inc.
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

#include <windows.h>
#include <ime.h>
#include <msctf.h>

#include <string>

#include "base/util.h"
#include "base/version.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "ipc/ipc_mock.h"
#include "session/commands.pb.h"
#include "session/ime_switch_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/ime/ime_core.h"
#include "win32/ime/ime_keyboard.h"

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

  virtual bool ForceTerminateServer(const string &name) {
    return true;
  }

  virtual bool WaitServer(uint32 pid) {
    return true;
  }

  virtual void OnFatal(ServerLauncherInterface::ServerErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
    error_map_[static_cast<int>(type)]++;
  }

  int error_count(ServerLauncherInterface::ServerErrorType type) {
    return error_map_[static_cast<int>(type)];
  }

  bool start_server_called() const {
    return start_server_called_;
  }

  void set_start_server_called(bool start_server_called) {
    start_server_called_ = start_server_called;
  }

  virtual void set_restricted(bool restricted) {}

  virtual void set_suppress_error_dialog(bool suppress) {}

  virtual void set_server_program(const string &server_path) {}

  virtual const string &server_program() const {
    static const string path;
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
  string response_;
  map<int, int> error_map_;
};

class MockClient : public client::Client {
 public:
  MockClient()
      : launcher_(NULL) {}
  explicit MockClient(const commands::Output &mock_response)
      : launcher_(NULL) {
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

  bool start_server_called() {
    return launcher_->start_server_called();
  }

 private:
  IPCClientFactoryMock client_factory_;
  TestServerLauncher *launcher_;

  DISALLOW_COPY_AND_ASSIGN(MockClient);
};
}  // anonymous namespace

TEST(ImeCoreTest, OpenIME) {
  commands::Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_elapsed_time(10);
  mock_output.set_mode(commands::FULL_KATAKANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::FULL_KATAKANA);

  MockClient mock_client(mock_output);
  const DWORD next_mode = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE |
                          IME_CMODE_ROMAN | IME_CMODE_KATAKANA;
  EXPECT_TRUE(ImeCore::OpenIME(&mock_client, next_mode));
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock_client.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::FULL_KATAKANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::ON, actual_input.key().special_key());
  }
}

TEST(ImeCoreTest, CloseIME) {
  commands::Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_elapsed_time(10);
  mock_output.set_mode(commands::DIRECT);
  mock_output.mutable_status()->set_activated(false);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);

  MockClient mock_client(mock_output);

  commands::Output output;
  EXPECT_TRUE(ImeCore::CloseIME(&mock_client, &output));
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock_client.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_FALSE(actual_input.key().has_mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::OFF, actual_input.key().special_key());
  }
}

TEST(ImeCoreTest, GetSupportableConversionMode) {
  EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE,
            ImeCore::GetSupportableConversionMode(
                IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE));

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

  EXPECT_NE(IME_SMODE_NONE,
            ImeCore::GetSupportableSentenceMode(MAXDWORD));
}

namespace {
// constants for unit tests
const DWORD kHiragana = IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
const DWORD kHalfAlpha = IME_CMODE_ALPHANUMERIC;

// Mozc uses only one candidate form.  This is why |kCandidateFormIndex| is
// always to be 1.
const DWORD kCandidateFormIndex = 1;

// Currently, Mozc always set 0 (L'\0') to the wparam of the
// WM_IME_COMPOSITION.
// TODO(yukawa): Support wparam of the WM_IME_COMPOSITION.
const wchar_t kLastUpdatedCharacter = L'\0';

const DWORD kCompositionUpdateFlag =
    GCS_COMPREADSTR | GCS_COMPREADATTR | GCS_COMPREADCLAUSE | GCS_COMPSTR |
    GCS_COMPATTR | GCS_COMPCLAUSE | GCS_CURSORPOS | GCS_DELTASTART;
COMPILE_ASSERT(0x1bf == kCompositionUpdateFlag, kCompositionUpdateFlagValue);

const DWORD kCompositionResultFlag =
    GCS_RESULTREADSTR | GCS_RESULTREADCLAUSE |
    GCS_RESULTSTR | GCS_RESULTCLAUSE;
COMPILE_ASSERT(0x1e00 == kCompositionResultFlag, kCompositionResultFlagValue);

const DWORD kCompositionResultAndUpdateFlag =
    kCompositionResultFlag | kCompositionUpdateFlag;
COMPILE_ASSERT(0x1FBF == kCompositionResultAndUpdateFlag,
               kkCompositionResultAndUpdateFlagValue);

const UIMessage kMsgMozcUIUpdate(WM_IME_NOTIFY, IMN_PRIVATE, kNotifyUpdateUI);
const UIMessage kMsgSetConversionMode(WM_IME_NOTIFY, IMN_SETCONVERSIONMODE, 0);
const UIMessage kMsgStartComposition(WM_IME_STARTCOMPOSITION, 0, 0);
const UIMessage kMsgCompositionUpdate(
    WM_IME_COMPOSITION, kLastUpdatedCharacter, kCompositionUpdateFlag);
const UIMessage kMsgCompositionResult(
    WM_IME_COMPOSITION, kLastUpdatedCharacter, kCompositionResultFlag);
const UIMessage kMsgCompositionResultAndUpdate(
    WM_IME_COMPOSITION, kLastUpdatedCharacter,
    kCompositionResultAndUpdateFlag);
const UIMessage kMsgEndComposition(WM_IME_ENDCOMPOSITION, 0, 0);

const UIMessage kMsgOpenCandidate(
    WM_IME_NOTIFY, IMN_OPENCANDIDATE, kCandidateFormIndex);
const UIMessage kMsgChangeCandidate(WM_IME_NOTIFY,
    IMN_CHANGECANDIDATE, kCandidateFormIndex);
const UIMessage kMsgCloseCandidate(WM_IME_NOTIFY,
    IMN_CLOSECANDIDATE, kCandidateFormIndex);

#define EXPECT_UI_MSG(expected_message, expected_wparam,                    \
                      expected_lparam, actual_ui_msg)                       \
  do {                                                                      \
     EXPECT_EQ((expected_message), (actual_ui_msg).message());              \
     EXPECT_EQ((expected_wparam), (actual_ui_msg).wparam());                \
     EXPECT_EQ((expected_lparam), (actual_ui_msg).lparam());                \
  } while (false)

#define EXPECT_UI_UPDATE_MSG(actual_msg)                                    \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_PRIVATE, kNotifyUpdateUI, (actual_msg))
#define EXPECT_SET_OPEN_STATUS_MSG(actual_msg)                              \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_SETOPENSTATUS, 0, (actual_msg))
#define EXPECT_SET_CONVERSION_MODE_MSG(actual_msg)                          \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_SETCONVERSIONMODE, 0, (actual_msg))
#define EXPECT_STARTCOMPOSITION_MSG(actual_msg)                             \
    EXPECT_UI_MSG(WM_IME_STARTCOMPOSITION, 0, 0, (actual_msg))
#define EXPECT_COMPOSITION_MSG(expected_flag, actual_msg)                   \
    EXPECT_UI_MSG(WM_IME_COMPOSITION, kLastUpdatedCharacter,                \
                  (expected_flag), (actual_msg))
#define EXPECT_ENDCOMPOSITION_MSG(actual_msg)                               \
    EXPECT_UI_MSG(WM_IME_ENDCOMPOSITION, 0, 0, (actual_msg))
#define EXPECT_OPENCANDIDATE_MSG(actual_msg)                                \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_OPENCANDIDATE, kCandidateFormIndex,    \
                  (actual_msg))
#define EXPECT_CHANGECANDIDATE_MSG(actual_msg)                              \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_CHANGECANDIDATE, kCandidateFormIndex,  \
                  (actual_msg))
#define EXPECT_CLOSECANDIDATE_MSG(actual_msg)                               \
    EXPECT_UI_MSG(WM_IME_NOTIFY, IMN_CLOSECANDIDATE, kCandidateFormIndex,   \
                  (actual_msg))
}  // anonymous namespace


// Check UI message order for
//   "Hankaku/Zenkaku" -> "(Shift +)G" -> "Hankaku/Zenkaku"
TEST(ImeCoreTest, TemporalConversionModeMessageOrderTest) {
  // "Hankaku/Zenkaku"
  {
    vector<UIMessage> composition_messages;
    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             false,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "(Shift +)G"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHalfAlpha,
                             &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[1]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "Hankaku/Zenkaku"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResult);
    composition_messages.push_back(kMsgEndComposition);
    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHalfAlpha,
                             false,
                             kHiragana,
                             &sorted_messages);

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
    vector<UIMessage> composition_messages;
    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             false,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "(Shift +)G"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHalfAlpha,
                             &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[1]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "o"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHalfAlpha,
                             true,
                             kHalfAlpha,
                             &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "Enter"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResult);
    composition_messages.push_back(kMsgEndComposition);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHalfAlpha,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(4, sorted_messages.size());

    EXPECT_SET_CONVERSION_MODE_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionResultFlag, sorted_messages[1]);
    EXPECT_ENDCOMPOSITION_MSG(sorted_messages[2]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[3]);
  }

  // "Hankaku/Zenkaku"
  {
    vector<UIMessage> composition_messages;
    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             false,
                             kHiragana,
                             &sorted_messages);

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
    vector<UIMessage> composition_messages;
    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             false,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());
    EXPECT_SET_OPEN_STATUS_MSG(sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "a"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgStartComposition);
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_STARTCOMPOSITION_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "Space"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(2, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[1]);
  }

  // "Space"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgOpenCandidate);

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_OPENCANDIDATE_MSG(sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "Space"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionUpdate);

    vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgChangeCandidate);

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());

    EXPECT_COMPOSITION_MSG(kCompositionUpdateFlag, sorted_messages[0]);
    EXPECT_CHANGECANDIDATE_MSG(sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }

  // "i"
  {
    vector<UIMessage> composition_messages;
    composition_messages.push_back(kMsgCompositionResultAndUpdate);

    vector<UIMessage> candidate_messages;
    candidate_messages.push_back(kMsgCloseCandidate);

    vector<UIMessage> sorted_messages;
    ImeCore::SortIMEMessages(composition_messages,
                             candidate_messages,
                             true,
                             kHiragana,
                             true,
                             kHiragana,
                             &sorted_messages);

    EXPECT_EQ(3, sorted_messages.size());
    // IMN_CLOSECANDIDATE must prior to any composition message!
    EXPECT_CLOSECANDIDATE_MSG(sorted_messages[0]);
    EXPECT_COMPOSITION_MSG(kCompositionResultAndUpdateFlag, sorted_messages[1]);
    EXPECT_UI_UPDATE_MSG(sorted_messages[2]);
  }
}
}  // namespace win32
}  // namespace mozc
