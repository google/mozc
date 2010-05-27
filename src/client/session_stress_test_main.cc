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

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

// TODO(taku)
// 1. multi-thread testing
// 2. change/config the senario

#include <iostream>
#include "base/base.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "client/session.h"
#include "renderer/renderer_client.h"
#include "session/commands.pb.h"

DEFINE_int32(max_keyevents, 100000,
             "test at most |max_keyevents| key sequences");
DEFINE_string(server_path, "", "specify server path");
DEFINE_int32(key_duration, 10, "key duration (msec)");
DEFINE_bool(display_preedit, true, "display predit to tty");
DEFINE_bool(test_renderer, false, "test renderer");
DEFINE_bool(test_testsendkey, true, "test TestSendKey");

DECLARE_bool(logtostderr);

namespace mozc {
namespace {

#include "client/session_stress_test_data.h"

const commands::KeyEvent::SpecialKey kSpecialKeys[] = {
  commands::KeyEvent::SPACE,
  commands::KeyEvent::BACKSPACE,
  commands::KeyEvent::DEL,
  commands::KeyEvent::DOWN,
  commands::KeyEvent::END,
  commands::KeyEvent::ENTER,
  commands::KeyEvent::ESCAPE,
  commands::KeyEvent::HOME,
  commands::KeyEvent::INSERT,
  commands::KeyEvent::HENKAN,
  commands::KeyEvent::MUHENKAN,
  commands::KeyEvent::LEFT,
  commands::KeyEvent::RIGHT,
  commands::KeyEvent::UP,
  commands::KeyEvent::DOWN,
  commands::KeyEvent::PAGE_UP,
  commands::KeyEvent::PAGE_DOWN,
  commands::KeyEvent::TAB,
  commands::KeyEvent::F1,
  commands::KeyEvent::F2,
  commands::KeyEvent::F3,
  commands::KeyEvent::F4,
  commands::KeyEvent::F5,
  commands::KeyEvent::F6,
  commands::KeyEvent::F7,
  commands::KeyEvent::F8,
  commands::KeyEvent::F9,
  commands::KeyEvent::F10,
  commands::KeyEvent::F11,
  commands::KeyEvent::F12
};

int GetRandom(int size) {
  return static_cast<int> (1.0 * size * rand() / (RAND_MAX + 1.0));
}

uint32 GetRandomAsciiKey() {
  return static_cast<uint32>(' ') + GetRandom(static_cast<uint32>('~' - ' '));
}

void GenerateSequence(vector<commands::KeyEvent> *keys) {
  CHECK(keys);
  keys->clear();

  const string sentence = kTestSentences[GetRandom(arraysize(kTestSentences))];
  CHECK(!sentence.empty());

  string tmp, input;
  Util::HiraganaToRomanji(sentence, &tmp);
  Util::FullWidthToHalfWidth(tmp, &input);

  VLOG(1) << input;

  vector<commands::KeyEvent> basic_keys;

  // generate basic input
  {
    // first add a normal keys
    const char *begin = input.data();
    const char *end = input.data() + input.size();
    while (begin < end) {
      size_t mblen = 0;
      const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen);
      CHECK_GT(mblen, 0);
      begin += mblen;
      if (ucs2 >= 0x20 && ucs2 <= 0x7F) {
        commands::KeyEvent key;
        key.set_key_code(ucs2);
        basic_keys.push_back(key);
      }
    }
  }

  // basic keys + conversion
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    for (int i = 0; i < 5; ++i) {
      const size_t num = GetRandom(30) + 8;
      for (size_t j = 0; j < num; ++j) {
        commands::KeyEvent key;
        key.set_special_key(commands::KeyEvent::SPACE);
        if (GetRandom(4) == 0) {
          key.add_modifier_keys(commands::KeyEvent::SHIFT);
          keys->push_back(key);
        }
      }
      commands::KeyEvent key;
      key.set_special_key(commands::KeyEvent::RIGHT);
      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // segment resize
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = GetRandom(30) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (GetRandom(4)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          if (GetRandom(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          if (GetRandom(2) == 0) {
            key.add_modifier_keys(commands::KeyEvent::SHIFT);
          }
          break;
        default:
          {
            const size_t space_num = GetRandom(20) + 3;
            for (size_t i = 0; i < space_num; ++i) {
              key.set_special_key(commands::KeyEvent::SPACE);
              keys->push_back(key);
            }
          }
          break;
      }

      if (GetRandom(4) == 0) {
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (GetRandom(10) == 0) {
        key.add_modifier_keys(commands::KeyEvent::ALT);
      }

      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // insert + delete
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      keys->push_back(basic_keys[i]);
    }

    const size_t num = GetRandom(20) + 10;
    for (size_t i = 0; i < num; ++i) {
      commands::KeyEvent key;
      switch (GetRandom(5)) {
        case 0:
          key.set_special_key(commands::KeyEvent::LEFT);
          break;
        case 1:
          key.set_special_key(commands::KeyEvent::RIGHT);
          break;
        case 2:
          key.set_special_key(commands::KeyEvent::DEL);
          break;
        case 3:
          key.set_special_key(commands::KeyEvent::BACKSPACE);
          break;
        default:
          {
            // add any ascii
            const size_t insert_num = GetRandom(5) + 1;
            for (size_t i = 0; i < insert_num; ++i) {
              key.set_key_code(GetRandomAsciiKey());
            }
          }
      }
      keys->push_back(key);
    }

    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  // basic keys + modifiers
  {
    for (size_t i = 0; i < basic_keys.size(); ++i) {
      commands::KeyEvent key;
      switch (GetRandom(8)) {
        case 0:
          key.set_key_code(kSpecialKeys[GetRandom(arraysize(kSpecialKeys))]);
          break;
        case 1:
          key.set_key_code(GetRandomAsciiKey());
          break;
        default:
          key.CopyFrom(basic_keys[i]);
          break;
      }

      if (GetRandom(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::CTRL);
      }

      if (GetRandom(10) == 0) {  // 10%
        key.add_modifier_keys(commands::KeyEvent::SHIFT);
      }

      if (GetRandom(50) == 0) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_DOWN);
      }

      if (GetRandom(50) == 0) {  // 2%
        key.add_modifier_keys(commands::KeyEvent::KEY_UP);
      }

      keys->push_back(key);
    }

    // submit
    commands::KeyEvent key;
    key.set_special_key(commands::KeyEvent::ENTER);
    keys->push_back(key);
  }

  CHECK_GT(keys->size(), 0);
  VLOG(1) << "key sequence is generated: " << keys->size();
}

void DisplayPreedit(const commands::Output &output) {
  // TODO(taku): display segment attributes
  if (output.has_preedit()) {
    string value;
    for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
      value += output.preedit().segment(i).value();
    }
#ifdef OS_WINDOWS
    string tmp;
    Util::UTF8ToSJIS(value, &tmp);
    cout << tmp << '\r';
#else
    cout << value << '\r';
#endif
  } else if (output.has_result()) {
#ifdef OS_WINDOWS
    string tmp;
    Util::UTF8ToSJIS(output.result().value(), &tmp);
    cout << tmp << endl;
#else
    cout << output.result().value() << endl;
#endif
  }
}
}  // namespace
}  // mozc

int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  FLAGS_logtostderr = true;

  mozc::client::Session client;
  if (!FLAGS_server_path.empty()) {
    client.set_server_program(FLAGS_server_path);
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not respoinding";

  scoped_ptr<mozc::renderer::RendererClient> renderer_client;
  mozc::commands::RendererCommand renderer_command;

  if (FLAGS_test_renderer) {
#if defined(OS_WINDOWS) || defined(OS_MACOSX)
#ifdef OS_WINDOWS
    renderer_command.mutable_application_info()->set_process_id
        (::GetCurrentProcessId());
    renderer_command.mutable_application_info()->set_thread_id
        (::GetCurrentThreadId());
#endif
    renderer_command.mutable_preedit_rectangle()->set_left(10);
    renderer_command.mutable_preedit_rectangle()->set_top(10);
    renderer_command.mutable_preedit_rectangle()->set_right(200);
    renderer_command.mutable_preedit_rectangle()->set_bottom(30);
#else
    LOG(FATAL) << "test_renderer is only supported on Windows and Mac";
#endif
    renderer_client.reset(new mozc::renderer::RendererClient);
    CHECK(renderer_client->Activate());
  }

  uint32 seed = 0;
  mozc::Util::GetSecureRandomSequence(reinterpret_cast<char *>(&seed),
                                      sizeof(seed));
  srand(seed);

  vector<mozc::commands::KeyEvent> keys;
  mozc::commands::Output output;
  int32 keyevents_size = 0;

  // TODO(taku):
  // Stop the test if server is crashed.
  // Currently, we cannot detect the server crash out of
  // client library, as client automatically re-lahches the server.

  while (true) {
    mozc::GenerateSequence(&keys);
    CHECK(client.NoOperation()) << "Server is not responding";
    for (size_t i = 0; i < keys.size(); ++i) {
      mozc::Util::Sleep(FLAGS_key_duration);
      keyevents_size++;
      if (keyevents_size % 100 == 0) {
        cout << keyevents_size << " key events finished" << endl;
      }
      if (FLAGS_max_keyevents < keyevents_size) {
        cout << "key events reached to " << FLAGS_max_keyevents << endl;
        return 0;
      }
      if (FLAGS_test_testsendkey) {
        VLOG(2) << "Sending to Server: " << keys[i].DebugString();
        client.TestSendKey(keys[i], &output);
        VLOG(2) << "Output of TestSendKey: " << output.DebugString();
        mozc::Util::Sleep(10);
      }

      VLOG(2) << "Sending to Server: " << keys[i].DebugString();
      client.SendKey(keys[i], &output);
      VLOG(2) << "Output of SendKey: " << output.DebugString();

      if (FLAGS_display_preedit) {
        mozc::DisplayPreedit(output);
      }

      if (renderer_client.get() != NULL) {
        renderer_command.set_type(mozc::commands::RendererCommand::UPDATE);
        renderer_command.set_visible(output.has_candidates());
        renderer_command.mutable_output()->CopyFrom(output);
        VLOG(2) << "Sending to Renderer: " << renderer_command.DebugString();
        renderer_client->ExecCommand(renderer_command);
      }
    }
  }

  return 0;
}
