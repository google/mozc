// Copyright 2010-2014, Google Inc.
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

#include <algorithm>
#include <string>
#include <vector>

#include "base/base.h"
#include "base/file_util.h"
#include "engine/engine_factory.h"
#include "session/commands.pb.h"
#include "session/random_keyevents_generator.h"
#include "session/session_handler.h"
#include "session/session_handler_test_util.h"
#include "testing/base/public/gunit.h"

#ifdef OS_ANDROID
#include "base/mmap.h"
#include "base/singleton.h"
#include "data_manager/android/android_data_manager.h"
#endif

namespace {
uint32 GenerateRandomSeed() {
  uint32 seed = 0;
  mozc::Util::GetRandomSequence(reinterpret_cast<char *>(&seed),
                                sizeof(seed));
  return seed;
}
}  // namespace

// There is no DEFINE_uint32.
DEFINE_uint64(random_seed, GenerateRandomSeed(),
              "Random seed value. "
              "This value will be interpreted as uint32.");
DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace mozc {

using session::testing::SessionHandlerTestBase;
using session::testing::TestSessionClient;

namespace {
#ifdef OS_ANDROID
// In actual libmozc.so usage, the dictionary data will be given via JNI call
// because only Java side code knows where the data is.
// On native code unittest, we cannot do it, so instead we mmap the files
// and use it.
// Note that this technique works here because the no other test code doesn't
// link to this binary.
// TODO(hidehiko): Get rid of this hack by refactoring Engine/DataManager
// related code.
class AndroidInitializer {
 private:
  AndroidInitializer() {
    string dictionary_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/dictionary_data");
    CHECK(dictionary_mmap_.Open(dictionary_data_path.c_str(), "r"));
    android::AndroidDataManager::SetDictionaryData(
        dictionary_mmap_.begin(), dictionary_mmap_.size());

    string connection_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/connection_data");
    CHECK(connection_mmap_.Open(connection_data_path.c_str(), "r"));
    android::AndroidDataManager::SetConnectionData(
        connection_mmap_.begin(), connection_mmap_.size());
    LOG(ERROR) << "mmap data initialized.";
  }

  friend class Singleton<AndroidInitializer>;

  Mmap dictionary_mmap_;
  Mmap connection_mmap_;

  DISALLOW_COPY_AND_ASSIGN(AndroidInitializer);
};
#endif  // OS_ANDROID
}  // namespace

class SessionHandlerStressTest : public SessionHandlerTestBase {
 protected:
  virtual EngineInterface *CreateEngine() {
#ifdef OS_ANDROID
    Singleton<AndroidInitializer>::get();
#endif  // OS_ANDROID
    return EngineFactory::Create();
  }
};

TEST_F(SessionHandlerStressTest, BasicStressTest) {
  vector<commands::KeyEvent> keys;
  commands::Output output;
  scoped_ptr<EngineInterface> engine(EngineFactory::Create());
  TestSessionClient client(engine.get());
  size_t keyevents_size = 0;
  const size_t kMaxEventSize = 10000;
  ASSERT_TRUE(client.CreateSession());

  const uint32 random_seed = static_cast<uint32>(FLAGS_random_seed);
  LOG(INFO) << "Random seed: " << random_seed;
  session::RandomKeyEventsGenerator::InitSeed(random_seed);
  while (keyevents_size < kMaxEventSize) {
    keys.clear();
    session::RandomKeyEventsGenerator::GenerateSequence(&keys);
    for (size_t i = 0; i < keys.size(); ++i) {
      ++keyevents_size;
      EXPECT_TRUE(client.TestSendKey(keys[i], &output));
      EXPECT_TRUE(client.SendKey(keys[i], &output));
    }
  }
  EXPECT_TRUE(client.DeleteSession());
}

}  // namespace mozc
