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

#include "dictionary/file/dictionary_file.h"

#include <cstdio>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "dictionary/file/codec_factory.h"
#include "dictionary/file/dictionary_file_builder.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace dictionary {
namespace {

TEST(DictionaryFileTest, Basic) {
  const std::string dfn = absl::GetFlag(FLAGS_test_tmpdir) + "/test-dictionary";
  const std::string fn1 = absl::GetFlag(FLAGS_test_tmpdir) + "/sec1";
  const std::string fn2 = absl::GetFlag(FLAGS_test_tmpdir) + "/sec2";

  FILE *fp1 = fopen(fn1.c_str(), "w");
  CHECK(fp1) << "failed to open temporary file";
  fwrite("0123456789", 10, 1, fp1);
  fclose(fp1);
  FILE *fp2 = fopen(fn2.c_str(), "w");
  CHECK(fp2) << "failed to open temporary file";
  fwrite("9876543210", 10, 1, fp2);
  fclose(fp2);

  {
    DictionaryFileBuilder builder(DictionaryFileCodecFactory::GetCodec());
    EXPECT_TRUE(builder.AddSectionFromFile("sec1", fn1));
    EXPECT_TRUE(builder.AddSectionFromFile("sec2", fn2));
    EXPECT_FALSE(builder.AddSectionFromFile("sec2", fn2));
    builder.WriteImageToFile(dfn);
  }

  EXPECT_TRUE(FileUtil::FileExists(dfn));

  {
    DictionaryFile df(DictionaryFileCodecFactory::GetCodec());
    ASSERT_TRUE(df.OpenFromFile(dfn).ok());
    int len;
    const char *ptr = df.GetSection("sec1", &len);
    EXPECT_EQ(10, len);
    std::string content(ptr, len);
    EXPECT_EQ("0123456789", content);
    ptr = df.GetSection("sec2", &len);
    EXPECT_EQ(10, len);
    content.assign(ptr, len);
    EXPECT_EQ("9876543210", content);
    ptr = df.GetSection("sec3", &len);
    EXPECT_TRUE(ptr == nullptr);
  }

  FileUtil::Unlink(dfn);
  FileUtil::Unlink(fn1);
  FileUtil::Unlink(fn2);
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
