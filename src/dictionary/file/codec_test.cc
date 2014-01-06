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

#include "dictionary/file/codec.h"

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

class CodecTest : public ::testing::Test {
 public:
  CodecTest() : test_file_(FLAGS_test_tmpdir + "testfile.txt") {}

 protected:
  virtual void SetUp() {
    DictionaryFileCodecFactory::SetCodec(NULL);
    FileUtil::Unlink(test_file_);
  }

  virtual void TearDown() {
    // Reset to default setting
    DictionaryFileCodecFactory::SetCodec(NULL);
    FileUtil::Unlink(test_file_);
  }

  void AddSection(const DictionaryFileCodecInterface *codec,
                  const string &name,
                  const char *ptr,
                  int len,
                  vector<DictionaryFileSection> *sections) const {
    CHECK(codec);
    CHECK(sections);
    sections->push_back(
        DictionaryFileSection(ptr, len, codec->GetSectionName(name)));
  }

  bool FindSection(const DictionaryFileCodecInterface *codec,
                   const vector<DictionaryFileSection> &sections,
                   const string &name,
                   int *index) const {
    CHECK(codec);
    CHECK(index);
    const string name_find = codec->GetSectionName(name);
    for (size_t i = 0; i < sections.size(); ++i) {
      if (sections[i].name == name_find) {
        *index = i;
        return true;
      }
    }
    return false;
  }

  bool CheckValue(const DictionaryFileSection &section,
                  const string &expected) const {
    const string value = string(section.ptr, section.len);
    return (expected == value);
  }

  const string test_file_;
};

class CodecMock : public DictionaryFileCodecInterface {
 public:
  virtual void WriteSections(const vector<DictionaryFileSection> &sections,
                             ostream *ofs) const {
    const string value = "dummy value";
    ofs->write(value.data(), value.size());
  }

  virtual bool ReadSections(const char *image, int length,
                            vector<DictionaryFileSection> *sections) const {
    sections->push_back(DictionaryFileSection(NULL, 0, "dummy name"));
    return true;
  }

  virtual string GetSectionName(const string &name) const {
    return "dummy section name";
  }
};

TEST_F(CodecTest, FactoryTest) {
  scoped_ptr<CodecMock> codec_mock(new CodecMock);
  DictionaryFileCodecFactory::SetCodec(codec_mock.get());
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  EXPECT_TRUE(codec != NULL);
  vector<DictionaryFileSection> sections;
  {
    OutputFileStream ofs;
    ofs.open(test_file_.c_str(), ios_base::out | ios_base::binary);
    codec->WriteSections(sections, &ofs);
  }
  {
    EXPECT_TRUE(FileUtil::FileExists(test_file_));
    InputFileStream ifs;
    ifs.open(test_file_.c_str(), ios_base::in | ios_base::binary);
    ifs.seekg(0, ios::end);
    const int len = ifs.tellg();
    ifs.seekg(0, ios::beg);
    char buf[64];
    ifs.read(buf, len);
    EXPECT_EQ("dummy value", string(buf, len));
  }
  {
    EXPECT_EQ(0, sections.size());
    EXPECT_TRUE(codec->ReadSections(NULL, 0, &sections));
    EXPECT_EQ(1, sections.size());
    EXPECT_EQ("dummy name", sections[0].name);
  }
  {
    EXPECT_EQ("dummy section name", codec->GetSectionName("test"));
  }
}

TEST_F(CodecTest, DefaultTest) {
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  EXPECT_TRUE(codec != NULL);
  {
    vector<DictionaryFileSection> write_sections;
    const string value0 = "Value 0 test";
    AddSection(codec, "Section 0", value0.data(), value0.size(),
               &write_sections);
    const string value1 = "Value 1 test test";
    AddSection(codec, "Section 1", value1.data(), value1.size(),
               &write_sections);
    OutputFileStream ofs;
    ofs.open(test_file_.c_str(), ios_base::out | ios_base::binary);
    codec->WriteSections(write_sections, &ofs);
  }
  char buf[1024] = {};  // sections will reference this buffer.
  vector<DictionaryFileSection> sections;
  {
    EXPECT_TRUE(FileUtil::FileExists(test_file_));
    InputFileStream ifs;
    ifs.open(test_file_.c_str(), ios_base::in | ios_base::binary);
    ifs.read(buf, 1024);
    EXPECT_TRUE(codec->ReadSections(buf, 1024, &sections));
  }
  EXPECT_EQ(2, sections.size());
  int index = -1;
  EXPECT_TRUE(FindSection(codec, sections, "Section 0", &index));
  EXPECT_TRUE(CheckValue(sections[index], "Value 0 test"));

  EXPECT_TRUE(FindSection(codec, sections, "Section 1", &index));
  EXPECT_TRUE(CheckValue(sections[index], "Value 1 test test"));
}

TEST_F(CodecTest, CodecTest) {
  scoped_ptr<DictionaryFileCodec> default_codec(
      new DictionaryFileCodec);
  DictionaryFileCodecFactory::SetCodec(default_codec.get());
  const DictionaryFileCodecInterface *codec =
      DictionaryFileCodecFactory::GetCodec();
  EXPECT_TRUE(codec != NULL);
  {
    vector<DictionaryFileSection> write_sections;
    const string value0 = "Value 0 test";
    AddSection(codec, "Section 0", value0.data(), value0.size(),
               &write_sections);
    const string value1 = "Value 1 test test";
    AddSection(codec, "Section 1", value1.data(), value1.size(),
               &write_sections);
    OutputFileStream ofs;
    ofs.open(test_file_.c_str(), ios_base::out | ios_base::binary);
    codec->WriteSections(write_sections, &ofs);
  }
  char buf[1024] = {};  // sections will reference this buffer.
  vector<DictionaryFileSection> sections;
  {
    EXPECT_TRUE(FileUtil::FileExists(test_file_));
    InputFileStream ifs;
    ifs.open(test_file_.c_str(), ios_base::in | ios_base::binary);
    ifs.read(buf, 1024);
    EXPECT_TRUE(codec->ReadSections(buf, 1024, &sections));
  }
  EXPECT_EQ(2, sections.size());
  int index = -1;
  EXPECT_TRUE(FindSection(codec, sections, "Section 0", &index));
  EXPECT_TRUE(CheckValue(sections[index], "Value 0 test"));

  EXPECT_TRUE(FindSection(codec, sections, "Section 1", &index));
  EXPECT_TRUE(CheckValue(sections[index], "Value 1 test test"));
}


}  // namespace
}  // namespace mozc
