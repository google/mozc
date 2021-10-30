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

#include "engine/engine_builder.h"

#include <string>

#include "base/file_util.h"
#include "prediction/predictor_interface.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace {

constexpr char kMockMagicNumber[] = "MOCK";

class EngineBuilderTest : public ::testing::Test {
 protected:
  EngineBuilderTest()
      : mock_data_path_(testing::GetSourcePath(
            {"data_manager", "testing", "mock_mozc.data"})) {}

  void Clear() {
    builder_.Clear();
    request_.Clear();
    response_.Clear();
  }

  const std::string mock_data_path_;
  EngineBuilder builder_;
  EngineReloadRequest request_;
  EngineReloadResponse response_;

 private:
  const testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
};

TEST_F(EngineBuilderTest, PrepareAsync) {
  {
    // Test request without install.
    request_.set_engine_type(EngineReloadRequest::MOBILE);
    request_.set_file_path(mock_data_path_);
    request_.set_magic_number(kMockMagicNumber);
    builder_.PrepareAsync(request_, &response_);
    ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

    builder_.Wait();
    ASSERT_TRUE(builder_.HasResponse());
    builder_.GetResponse(&response_);
    EXPECT_EQ(EngineReloadResponse::RELOAD_READY, response_.status());
  }
  Clear();
  {
    // Test request with install.  Since the requested file is copied,
    // |mock_data_path_| is copied to a temporary file.
    const std::string src_path =
        FileUtil::JoinPath({absl::GetFlag(FLAGS_test_tmpdir), "src.data"});
    ASSERT_OK(FileUtil::CopyFile(mock_data_path_, src_path));

    const std::string install_path =
        FileUtil::JoinPath({absl::GetFlag(FLAGS_test_tmpdir), "dst.data"});
    request_.set_engine_type(EngineReloadRequest::MOBILE);
    request_.set_file_path(src_path);
    request_.set_install_location(install_path);
    request_.set_magic_number(kMockMagicNumber);
    builder_.PrepareAsync(request_, &response_);
    ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

    builder_.Wait();
    ASSERT_TRUE(builder_.HasResponse());
    builder_.GetResponse(&response_);
    EXPECT_EQ(EngineReloadResponse::RELOAD_READY, response_.status());
    // Verify |src_path| was copied.
    EXPECT_OK(FileUtil::FileExists(src_path));
    EXPECT_OK(FileUtil::FileExists(install_path));
  }
}

TEST_F(EngineBuilderTest, AsyncBuildWithoutInstall) {
  struct {
    EngineReloadRequest::EngineType type;
    const char *predictor_name;
  } kTestCases[] = {
      {EngineReloadRequest::DESKTOP, "DefaultPredictor"},
      {EngineReloadRequest::MOBILE, "MobilePredictor"},
  };

  for (const auto &test_case : kTestCases) {
    Clear();

    // Request preparation without install.
    request_.set_engine_type(test_case.type);
    request_.set_file_path(mock_data_path_);
    request_.set_magic_number(kMockMagicNumber);
    builder_.PrepareAsync(request_, &response_);
    ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

    builder_.Wait();

    // Builder should be ready now.
    ASSERT_TRUE(builder_.HasResponse());
    builder_.GetResponse(&response_);
    ASSERT_EQ(EngineReloadResponse::RELOAD_READY, response_.status());

    // Build an engine and verify its predictor type (desktop or mobile).
    auto engine = builder_.BuildFromPreparedData();
    ASSERT_TRUE(engine);
    EXPECT_EQ(test_case.predictor_name,
              engine->GetPredictor()->GetPredictorName());

    // Cannot build twice.
    engine = builder_.BuildFromPreparedData();
    EXPECT_FALSE(engine);
  }
}

TEST_F(EngineBuilderTest, AsyncBuildWithInstall) {
  struct {
    EngineReloadRequest::EngineType type;
    const char *predictor_name;
  } kTestCases[] = {
      {EngineReloadRequest::DESKTOP, "DefaultPredictor"},
      {EngineReloadRequest::MOBILE, "MobilePredictor"},
  };
  const std::string &tmp_src =
      FileUtil::JoinPath({absl::GetFlag(FLAGS_test_tmpdir), "src.data"});
  const std::string install_path =
      FileUtil::JoinPath({absl::GetFlag(FLAGS_test_tmpdir), "dst.data"});

  for (const auto &test_case : kTestCases) {
    Clear();

    // Since requested file is copied, copy |mock_data_path_| to a temporary
    // file.
    ASSERT_OK(FileUtil::CopyFile(mock_data_path_, tmp_src));

    // Request preparation with install.
    request_.set_engine_type(test_case.type);
    request_.set_file_path(tmp_src);
    request_.set_install_location(install_path);
    request_.set_magic_number(kMockMagicNumber);
    builder_.PrepareAsync(request_, &response_);
    ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

    builder_.Wait();

    // Builder should be ready now.
    ASSERT_TRUE(builder_.HasResponse());
    builder_.GetResponse(&response_);
    ASSERT_EQ(EngineReloadResponse::RELOAD_READY, response_.status());

    // |tmp_src| should be copied to |install_path|.
    ASSERT_OK(FileUtil::FileExists(tmp_src));
    ASSERT_OK(FileUtil::FileExists(install_path));

    // Build an engine and verify its predictor type (desktop or mobile).
    auto engine = builder_.BuildFromPreparedData();
    ASSERT_TRUE(engine);
    EXPECT_EQ(test_case.predictor_name,
              engine->GetPredictor()->GetPredictorName());

    // Cannot build twice.
    engine = builder_.BuildFromPreparedData();
    EXPECT_FALSE(engine);
  }
}

TEST_F(EngineBuilderTest, FailureCase_DataBroken) {
  // Test the case where input file is invalid.
  request_.set_engine_type(EngineReloadRequest::MOBILE);
  request_.set_file_path(
      testing::GetSourceFileOrDie({"engine", "engine_builder_test.cc"}));
  request_.set_magic_number(kMockMagicNumber);
  builder_.PrepareAsync(request_, &response_);
  ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

  builder_.Wait();

  ASSERT_TRUE(builder_.HasResponse());
  builder_.GetResponse(&response_);
  ASSERT_EQ(EngineReloadResponse::DATA_BROKEN, response_.status());
}

TEST_F(EngineBuilderTest, FailureCase_FileDoesNotExist) {
  // Test the case where input file doesn't exist.
  request_.set_engine_type(EngineReloadRequest::MOBILE);
  request_.set_file_path("file_does_not_exist");
  request_.set_magic_number(kMockMagicNumber);
  builder_.PrepareAsync(request_, &response_);
  ASSERT_EQ(EngineReloadResponse::ACCEPTED, response_.status());

  builder_.Wait();

  ASSERT_TRUE(builder_.HasResponse());
  builder_.GetResponse(&response_);
  ASSERT_EQ(EngineReloadResponse::MMAP_FAILURE, response_.status());
}

}  // namespace
}  // namespace mozc
