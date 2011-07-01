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

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);

namespace mozc {

TEST(ConfigHandlerTest, SetConfig) {
  config::Config input;
  config::Config output;

  const string config_file = Util::JoinPath(FLAGS_test_tmpdir,
                                            "mozc_config_test_tmp");
  Util::Unlink(config_file);
  config::ConfigHandler::SetConfigFileName(config_file);
  EXPECT_EQ(config_file, config::ConfigHandler::GetConfigFileName());
  config::ConfigHandler::Reload();

  config::ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(true);
#ifndef NO_LOGGING
  input.set_verbose_level(2);
#endif  // NO_LOGGING
  config::ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));
  input.set_last_modified_time(0);
  output.set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());

  config::ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(false);
#ifndef NO_LOGGING
  input.set_verbose_level(0);
#endif  // NO_LOGGING
  config::ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(config::ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(config::ConfigHandler::GetConfig(&output));

  input.set_last_modified_time(0);
  output.set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());
}

TEST(ConfigHandlerTest, ConfigFileNameConfig) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);

  const string config_file = string("config")
      + Util::SimpleItoa(config::CONFIG_VERSION);

  const string filename = Util::JoinPath(FLAGS_test_tmpdir,
                                         config_file);
  Util::Unlink(filename);
  config::Config input;
  config::ConfigHandler::SetConfig(input);
  Util::FileExists(filename);
}
}
