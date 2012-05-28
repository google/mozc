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

#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "rewriter/version_rewriter.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

class VersionRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    // Reserve the previous request before starting a test.
    previous_request_.CopyFrom(commands::RequestHandler::GetRequest());
  }

  virtual void TearDown() {
    // and gets back to the request at the end of a test.
    commands::RequestHandler::SetRequest(previous_request_);
  }

 private:
  commands::Request previous_request_;
};

TEST_F(VersionRewriterTest, CapabilityTest) {
  // default_request is just declared but not touched at all, so it
  // holds all default values.
  commands::Request default_request;
  commands::RequestHandler::SetRequest(default_request);
  VersionRewriter rewriter;
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability());
}

TEST_F(VersionRewriterTest, MobileEnvironmentTest) {
  commands::Request input;
  VersionRewriter rewriter;

  input.set_mixed_conversion(true);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::ALL, rewriter.capability());

  input.set_mixed_conversion(false);
  commands::RequestHandler::SetRequest(input);
  EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability());
}
}  // namespace mozc
