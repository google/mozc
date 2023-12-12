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

#include "base/codegen_bytearray_stream.h"

#include <cstddef>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

class CodeGenByteArrayStreamTest : public testing::Test {
 protected:
  CodeGenByteArrayStreamTest() : codegen_stream_(result_stream_) {}

  std::string ExpectedOutput(const absl::string_view var_name_base,
                             const size_t count, const absl::string_view body) {
    return absl::StrFormat(
        "alignas(std::max_align_t) constexpr char k%s_data[] = {%s};\n"
        "constexpr size_t k%s_size = %d;\n",
        var_name_base, body, var_name_base, count);
  }

  std::string ResultOutput() { return result_stream_.str(); }

  std::ostringstream result_stream_;
  CodeGenByteArrayOutputStream codegen_stream_;
};

TEST_F(CodeGenByteArrayStreamTest, NoInput) {
  codegen_stream_.OpenVarDef("NoInput");
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("NoInput", 0, "");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, EmptyString) {
  codegen_stream_.OpenVarDef("EmptyString");
  codegen_stream_ << "";
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("EmptyString", 0, "");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleByte) {
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_ << '\001';
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("Test", 1, "\n0x01,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleByteZero) {
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_ << '\000';
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("Test", 1, "\n0x00,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleWord) {
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_ << "12345678";
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput(
      "Test", 8, "\n0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleLine) {
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_ << "0123456789abcdefghij";
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput(
      "Test", 20,
      "\n"
      "0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, "
      "0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, SingleLinePlusOne) {
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_ << "0123456789abcdefghij\xFF";
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput(
      "Test", 21,
      "\n"
      "0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, "
      "0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,\n"
      "0xFF,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, FragmentaryFlush) {
  codegen_stream_.OpenVarDef("Test");
  const char input_data[] = "12345678";
  for (int i = 0; input_data[i]; ++i) {
    codegen_stream_ << input_data[i];
    codegen_stream_.flush();
  }
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput(
      "Test", 8, "\n0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,\n");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, MultipleVarDefs) {
  codegen_stream_.OpenVarDef("First");
  codegen_stream_ << "12345678";
  codegen_stream_.CloseVarDef();

  codegen_stream_.OpenVarDef("Second");
  codegen_stream_ << "abcdefgh";
  codegen_stream_.CloseVarDef();

  const std::string expected = absl::StrCat(
      ExpectedOutput("First", 8,
                     "\n0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,\n"),
      ExpectedOutput("Second", 8,
                     "\n0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,\n"));
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

// error cases

TEST_F(CodeGenByteArrayStreamTest, OpenDoubly) {
  EXPECT_TRUE(codegen_stream_.good());
  codegen_stream_.OpenVarDef("Test1");
  codegen_stream_.OpenVarDef("Test2");
  EXPECT_FALSE(codegen_stream_.good());

  // Recover from the above error.
  codegen_stream_.clear();
  EXPECT_TRUE(codegen_stream_.good());
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("Test1", 0, "");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, CloseBeforeOpen) {
  EXPECT_TRUE(codegen_stream_.good());
  codegen_stream_.CloseVarDef();
  EXPECT_FALSE(codegen_stream_.good());

  // Recover from the above error.
  codegen_stream_.clear();
  EXPECT_TRUE(codegen_stream_.good());

  codegen_stream_.OpenVarDef("Test");
  codegen_stream_.CloseVarDef();

  const std::string expected = ExpectedOutput("Test", 0, "");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, CloseDoubly) {
  EXPECT_TRUE(codegen_stream_.good());
  codegen_stream_.OpenVarDef("Test");
  codegen_stream_.CloseVarDef();
  codegen_stream_.CloseVarDef();
  EXPECT_FALSE(codegen_stream_.good());

  // Recover from the above error.
  codegen_stream_.clear();

  const std::string expected = ExpectedOutput("Test", 0, "");
  EXPECT_EQ(ResultOutput(), expected);
  EXPECT_TRUE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, FlushBeforeOpen) {
  EXPECT_TRUE(codegen_stream_.good());
  codegen_stream_ << "hello, world" << std::endl;
  EXPECT_FALSE(codegen_stream_.good());
}

TEST_F(CodeGenByteArrayStreamTest, Move) {
  std::ostringstream oss;
  CodeGenByteArrayOutputStream stream(oss);
  stream.OpenVarDef("Test1");
  stream << '\x00';
  stream.setstate(std::ios::failbit);
  CodeGenByteArrayOutputStream stream2(std::move(stream));
  EXPECT_FALSE(stream2.good());
  stream2.clear();
  stream2.CloseVarDef();
  EXPECT_EQ(oss.str(), ExpectedOutput("Test1", 1, "\n0x00,\n"));
  stream2.OpenVarDef("Test2");
  stream2 << '\x01';
  stream = std::move(stream2);
  stream.CloseVarDef();
  EXPECT_TRUE(stream.good());
  EXPECT_EQ(oss.str(), absl::StrCat(ExpectedOutput("Test1", 1, "\n0x00,\n"),
                                    ExpectedOutput("Test2", 1, "\n0x01,\n")));
  stream.CloseVarDef();
  EXPECT_FALSE(stream.good());
  stream2 = std::move(stream);
  EXPECT_FALSE(stream2.good());
}

}  // namespace
}  // namespace mozc
