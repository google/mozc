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

#include "dictionary/file/codec.h"

#include <cstddef>
#include <ios>
#include <ostream>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/file/temp_dir.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "dictionary/file/codec_factory.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace dictionary {
namespace {

class CodecTest : public ::testing::Test {
 public:
  CodecTest() : test_file_(testing::MakeTempFileOrDie()) {}

 protected:
  void SetUp() override { DictionaryFileCodecFactory::SetCodec(nullptr); }

  void TearDown() override {
    // Reset to default setting
    DictionaryFileCodecFactory::SetCodec(nullptr);
  }

  void AddSection(const DictionaryFileCodecInterface *codec,
                  const absl::string_view name, const char *ptr, int len,
                  std::vector<DictionaryFileSection> *sections) const {
    CHECK(codec);
    CHECK(sections);
    sections->push_back(
        DictionaryFileSection(ptr, len, codec->GetSectionName(name)));
  }

  bool FindSection(const DictionaryFileCodecInterface *codec,
                   absl::Span<const DictionaryFileSection> sections,
                   const absl::string_view name, int *index) const {
    CHECK(codec);
    CHECK(index);
    const std::string name_find = codec->GetSectionName(name);
    for (size_t i = 0; i < sections.size(); ++i) {
      if (sections[i].name == name_find) {
        *index = i;
        return true;
      }
    }
    return false;
  }

  bool CheckValue(const DictionaryFileSection &section,
                  const absl::string_view expected) const {
    const std::string value(section.ptr, section.len);
    return (expected == value);
  }

  TempFile test_file_;
};

class CodecMock : public DictionaryFileCodecInterface {
 public:
  void WriteSections(absl::Span<const DictionaryFileSection> sections,
                     std::ostream *ofs) const override {
    const std::string value = "placeholder value";
    ofs->write(value.data(), value.size());
  }

  absl::Status ReadSections(
      const char *image, int length,
      std::vector<DictionaryFileSection> *sections) const override {
    sections->emplace_back(nullptr, 0, "placeholder name");
    return absl::Status();
  }

  std::string GetSectionName(const absl::string_view name) const override {
    return "placeholder section name";
  }
};

TEST_F(CodecTest, FactoryTest) {
  CodecMock codec_mock;
  DictionaryFileCodecFactory::SetCodec(&codec_mock);
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  ASSERT_NE(codec, nullptr);
  std::vector<DictionaryFileSection> sections;
  {
    OutputFileStream ofs;
    ofs.open(test_file_.path(), std::ios_base::out | std::ios_base::binary);
    codec->WriteSections(sections, &ofs);
  }
  {
    absl::StatusOr<std::string> content =
        FileUtil::GetContents(test_file_.path());
    ASSERT_OK(content);
    EXPECT_EQ(*content, "placeholder value");
  }
  {
    EXPECT_EQ(sections.size(), 0);
    EXPECT_OK(codec->ReadSections(nullptr, 0, &sections));
    EXPECT_EQ(sections.size(), 1);
    EXPECT_EQ(sections[0].name, "placeholder name");
  }
  { EXPECT_EQ(codec->GetSectionName("test"), "placeholder section name"); }
}

TEST_F(CodecTest, DefaultTest) {
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  ASSERT_NE(codec, nullptr);
  {
    std::vector<DictionaryFileSection> write_sections;
    const std::string value0 = "Value 0 test";
    AddSection(codec, "Section 0", value0.data(), value0.size(),
               &write_sections);
    const std::string value1 = "Value 1 test test";
    AddSection(codec, "Section 1", value1.data(), value1.size(),
               &write_sections);
    OutputFileStream ofs;
    ofs.open(test_file_.path(), std::ios_base::out | std::ios_base::binary);
    codec->WriteSections(write_sections, &ofs);
  }
  // sections will reference this buffer.
  absl::StatusOr<std::string> buf = FileUtil::GetContents(test_file_.path());
  std::vector<DictionaryFileSection> sections;
  ASSERT_OK(buf);
  ASSERT_OK(codec->ReadSections(buf->data(), buf->size(), &sections));
  ASSERT_EQ(2, sections.size());
  int index = -1;
  ASSERT_TRUE(FindSection(codec, sections, "Section 0", &index));
  ASSERT_EQ(0, index);
  EXPECT_TRUE(CheckValue(sections[index], "Value 0 test"));

  ASSERT_TRUE(FindSection(codec, sections, "Section 1", &index));
  ASSERT_EQ(1, index);
  EXPECT_TRUE(CheckValue(sections[index], "Value 1 test test"));
}

TEST_F(CodecTest, RandomizedCodecTest) {
  DictionaryFileCodec internal_codec;
  DictionaryFileCodecFactory::SetCodec(&internal_codec);
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  ASSERT_NE(codec, nullptr);
  {
    std::vector<DictionaryFileSection> write_sections;
    const std::string value0 = "Value 0 test";
    AddSection(codec, "Section 0", value0.data(), value0.size(),
               &write_sections);
    const std::string value1 = "Value 1 test test";
    AddSection(codec, "Section 1", value1.data(), value1.size(),
               &write_sections);
    OutputFileStream ofs;
    ofs.open(test_file_.path(), std::ios_base::out | std::ios_base::binary);
    codec->WriteSections(write_sections, &ofs);
  }
  // sections will reference this buffer.
  absl::StatusOr<std::string> buf = FileUtil::GetContents(test_file_.path());
  std::vector<DictionaryFileSection> sections;
  ASSERT_OK(buf);
  ASSERT_OK(codec->ReadSections(buf->data(), buf->size(), &sections));
  ASSERT_EQ(2, sections.size());
  int index = -1;
  ASSERT_TRUE(FindSection(codec, sections, "Section 0", &index));
  ASSERT_EQ(0, index);
  EXPECT_TRUE(CheckValue(sections[index], "Value 0 test"));

  ASSERT_TRUE(FindSection(codec, sections, "Section 1", &index));
  ASSERT_EQ(1, index);
  EXPECT_TRUE(CheckValue(sections[index], "Value 1 test test"));
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
