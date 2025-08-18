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

// Provides CodeGenByteArrayStream class which generates C/C++ source code
// to define a byte array as a C string literal.

#ifndef MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_
#define MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_

#include <cstddef>
#include <ios>
#include <ostream>
#include <streambuf>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"

#ifdef __ANDROID__
// This is used only for code generation, so shouldn't be used from android
// platform.
#error \
    "base/codegen_bytearray_stream.h shouldn't be used from android platform."
#endif  // __ANDROID__

namespace mozc {

class BasicCodeGenByteArrayStreamBuf : public std::streambuf {
 public:
  // Args:
  //   output_stream: The output stream to which generated code is written.
  explicit BasicCodeGenByteArrayStreamBuf(
      std::ostream& output_stream ABSL_ATTRIBUTE_LIFETIME_BOUND);

  BasicCodeGenByteArrayStreamBuf(BasicCodeGenByteArrayStreamBuf&&) = default;
  BasicCodeGenByteArrayStreamBuf& operator=(BasicCodeGenByteArrayStreamBuf&&) =
      default;

  // Writes the beginning of a variable definition.
  bool OpenVarDef(absl::string_view var_name_base);

  // Writes the end of a variable definition.
  bool CloseVarDef();

 protected:
  // Flushes.
  int sync() override;

  // Writes a given character sequence.  The implementation is expected to be
  // more efficient than the one of the base class.
  std::streamsize xsputn(const char_type* s, std::streamsize n) override;

  // Writes the data body of a variable definition.
  int overflow(int c) override;

 private:
  static constexpr size_t kDefaultInternalBufferSize = 4000 * 1024;  // 4 MB
  static constexpr size_t kNumOfBytesOnOneLine = 20;

  // Converts a raw byte stream to C source code.
  void WriteBytes(const char_type* begin, const char_type* end);

  std::vector<char_type> internal_output_buffer_;
  std::ostream* output_stream_;
  bool is_open_;
  std::string var_name_base_;
  size_t output_count_;
};

// Generates C/C++ source code to define a byte array as a C string literal.
//
// The generated code with a string literal looks like:
//   alignas(std::max_align_t) constexpr char kVAR_data[] = {
//   0x12, 0x34, 0x56, 0x78, ...,
//   };
//   constexpr size_t kVAR_size = 123;
//
// Usage:
//   ofstream ofs("output.cc");
//   CodeGenByteArrayOutputStream codegen_stream(ofs);
//   codegen_stream.OpenVarDef("MyVar");
//   codegen_stream.put(single_byte_data);
//   codegen_stream.write(large_data, large_data_size);
//   codegen_stream.CloseVarDef();
class CodeGenByteArrayOutputStream : public std::ostream {
 public:
  // Args:
  //   output_stream: The output stream to which generated code is written.
  explicit CodeGenByteArrayOutputStream(
      std::ostream& output_stream ABSL_ATTRIBUTE_LIFETIME_BOUND);

  CodeGenByteArrayOutputStream(CodeGenByteArrayOutputStream&& other) noexcept;
  CodeGenByteArrayOutputStream& operator=(
      CodeGenByteArrayOutputStream&& other) noexcept;

  // Writes the beginning of a variable definition.
  // A call to |OpenVarDef| must precede any output to the instance.
  void OpenVarDef(absl::string_view var_name_base);

  // Writes the end of a variable definition.
  // An output to the instance after a call to |CloseVarDef| is not allowed
  // unless |OpenVarDef| is called with a different variable name.
  void CloseVarDef();

 private:
  BasicCodeGenByteArrayStreamBuf streambuf_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_
