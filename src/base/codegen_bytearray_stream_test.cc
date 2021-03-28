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

// NOTE: You can test the word array version of the implementation of
// BasicCodeGenByteArrayStreamBuf on non-Windows platforms by defining
// MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY macro.
//
// Uncomment out the following line if you want.
//
//   #define MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY

#include "base/codegen_bytearray_stream.h"

#include <memory>
#include <sstream>

#include "base/port.h"
#include "testing/base/public/gunit.h"
#include "absl/memory/memory.h"

namespace {

class CodeGenByteArrayStreamTest : public testing::Test {
 protected:
  void SetUp() override {
    result_stream_ = absl::make_unique<std::ostringstream>();
    codegen_stream_ = absl::make_unique<mozc::CodeGenByteArrayOutputStream>(
        result_stream_.get(), mozc::codegenstream::NOT_OWN_STREAM);
  }

  void TearDown() override {
    codegen_stream_.reset();
    result_stream_.reset();
  }

  std::string ExpectedOutput(const std::string &var_name_base,
                             const std::string &count,
                             const std::string &body) {
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
    return "const uint64 k" + var_name_base + "_data_wordtype[] = {\n" +
        body + "};\n"
        "const char * const k" + var_name_base + "_data = "
        "reinterpret_cast<const char *>("
        "k" + var_name_base + "_data_wordtype);\n"
        "const size_t k" + var_name_base + "_size = " + count + ";\n";
#else
    return "const char k" + var_name_base + "_data[] =\n" +
        body + "\n"
        ";\n"
        "const size_t k" + var_name_base + "_size = " + count + ";\n";
#endif
  }

  std::string ResultOutput() { return result_stream_->str(); }

  std::unique_ptr<mozc::CodeGenByteArrayOutputStream> codegen_stream_;
  std::unique_ptr<std::ostringstream> result_stream_;
};

TEST_F(CodeGenByteArrayStreamTest, NoInput) {
  codegen_stream_->OpenVarDef("NoInput");
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected = ExpectedOutput("NoInput", "0", "");
#else
  const std::string expected = ExpectedOutput("NoInput", "0", "\"\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, EmptyString) {
  codegen_stream_->OpenVarDef("EmptyString");
  *codegen_stream_ << "";
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected = ExpectedOutput("EmptyString", "0", "");
#else
  const std::string expected = ExpectedOutput("EmptyString", "0", "\"\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleByte) {
  codegen_stream_->OpenVarDef("Test");
  *codegen_stream_ << '\001';
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "1", "0x0000000000000001");
#else
  const std::string expected = ExpectedOutput("Test", "1", "\"\\x01\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleByteZero) {
  codegen_stream_->OpenVarDef("Test");
  *codegen_stream_ << '\000';
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "1", "0x0000000000000000");
#else
  const std::string expected = ExpectedOutput("Test", "1", "\"\\x00\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleWord) {
  codegen_stream_->OpenVarDef("Test");
  *codegen_stream_ << "12345678";
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "8", "0x3837363534333231, ");
#else
  const std::string expected = ExpectedOutput(
      "Test", "8", "\"\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleLine) {
  codegen_stream_->OpenVarDef("Test");
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  *codegen_stream_ << "12345678ABCDEFGHabcdefghIJKLMNOP";
#else
  *codegen_stream_ << "0123456789abcdefghij";
#endif
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "32",
                     "0x3837363534333231, 0x4847464544434241, "
                     "0x6867666564636261, 0x504F4E4D4C4B4A49,\n");
#else
  const std::string expected =
      ExpectedOutput("Test", "20",
                     "\"\\x30\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\\x39"
                     "\\x61\\x62\\x63\\x64\\x65\\x66\\x67\\x68\\x69\\x6A\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleLinePlusOne) {
  codegen_stream_->OpenVarDef("Test");
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  *codegen_stream_ << "12345678ABCDEFGHabcdefghIJKLMNOP\xFF";
#else
  *codegen_stream_ << "0123456789abcdefghij\xFF";
#endif
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "33",
                     "0x3837363534333231, 0x4847464544434241, "
                     "0x6867666564636261, 0x504F4E4D4C4B4A49,\n"
                     "0x00000000000000FF");
#else
  const std::string expected =
      ExpectedOutput("Test", "21",
                     "\"\\x30\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\\x39"
                     "\\x61\\x62\\x63\\x64\\x65\\x66\\x67\\x68\\x69\\x6A\"\n"
                     "\"\\xFF\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, FragmentaryFlush) {
  codegen_stream_->OpenVarDef("Test");
  const char input_data[] = "12345678";
  for (int i = 0; input_data[i]; ++i) {
    *codegen_stream_ << input_data[i];
    codegen_stream_->flush();
  }
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("Test", "8", "0x3837363534333231, ");
#else
  const std::string expected = ExpectedOutput(
      "Test", "8", "\"\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, MultipleVarDefs) {
  codegen_stream_->OpenVarDef("First");
  *codegen_stream_ << "12345678";
  codegen_stream_->CloseVarDef();

  codegen_stream_->OpenVarDef("Second");
  *codegen_stream_ << "abcdefgh";
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected =
      ExpectedOutput("First", "8", "0x3837363534333231, ") +
      ExpectedOutput("Second", "8", "0x6867666564636261, ");
#else
  const std::string expected =
      ExpectedOutput("First", "8",
                     "\"\\x31\\x32\\x33\\x34\\x35\\x36\\x37\\x38\"") +
      ExpectedOutput("Second", "8",
                     "\"\\x61\\x62\\x63\\x64\\x65\\x66\\x67\\x68\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

// error cases

TEST_F(CodeGenByteArrayStreamTest, OpenDoubly) {
  EXPECT_TRUE(codegen_stream_->good());
  codegen_stream_->OpenVarDef("Test1");
  codegen_stream_->OpenVarDef("Test2");
  EXPECT_FALSE(codegen_stream_->good());

  // Recover from the above error.
  codegen_stream_->clear();
  EXPECT_TRUE(codegen_stream_->good());
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected = ExpectedOutput("Test1", "0", "");
#else
  const std::string expected = ExpectedOutput("Test1", "0", "\"\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, CloseBeforeOpen) {
  EXPECT_TRUE(codegen_stream_->good());
  codegen_stream_->CloseVarDef();
  EXPECT_FALSE(codegen_stream_->good());

  // Recover from the above error.
  codegen_stream_->clear();
  EXPECT_TRUE(codegen_stream_->good());

  codegen_stream_->OpenVarDef("Test");
  codegen_stream_->CloseVarDef();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected = ExpectedOutput("Test", "0", "");
#else
  const std::string expected = ExpectedOutput("Test", "0", "\"\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, CloseDoubly) {
  EXPECT_TRUE(codegen_stream_->good());
  codegen_stream_->OpenVarDef("Test");
  codegen_stream_->CloseVarDef();
  codegen_stream_->CloseVarDef();
  EXPECT_FALSE(codegen_stream_->good());

  // Recover from the above error.
  codegen_stream_->clear();

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  const std::string expected = ExpectedOutput("Test", "0", "");
#else
  const std::string expected = ExpectedOutput("Test", "0", "\"\"");
#endif
  EXPECT_EQ(expected, ResultOutput());
  EXPECT_TRUE(codegen_stream_->good());
}

TEST_F(CodeGenByteArrayStreamTest, FlushBeforeOpen) {
  EXPECT_TRUE(codegen_stream_->good());
  *codegen_stream_ << "hello, world" << std::endl;
  EXPECT_FALSE(codegen_stream_->good());
}

}  // namespace
