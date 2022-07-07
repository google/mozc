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
//
// Usage:
//   ofstream ofs("output.cc");
//   CodeGenByteArrayOutputStream codegen_stream(&ofs,
//                                               codegenstream::NOT_OWN_STREAM);
//   codegen_stream.OpenVarDef("MyVar");
//   codegen_stream.put(single_byte_data);
//   codegen_stream.write(large_data, large_data_size);
//   codegen_stream.CloseVarDef();

#ifndef MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_
#define MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_

#include <algorithm>
#include <ios>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>

#include "base/port.h"

#ifdef OS_ANDROID
// This is used only for code generation, so shouldn't be used from android
// platform.
#error \
    "base/codegen_bytearray_stream.h shouldn't be used from android platform."
#endif

#ifdef OS_WIN
// Visual C++ does not support string literals longer than 65535 characters
// so integer arrays (e.g. arrays of uint64) are used to represent byte arrays
// on Windows.
//
// The generated code looks like:
//   const uint64 kVAR_data_wordtype[] = {
//       0x0123456789ABCDEF, ...
//   };
//   const char * const kVAR_data =
//       reinterpret_cast<const char *>(kVAR_data_wordtype);
//   const size_t kVAR_size = 123;
//
// This implementation works well with other toolchains, too, but we use
// string literals for other toolchains.
//
// The generated code with a string literal looks like:
//   const char kVAR_data[] =
//       "\\x12\\x34\\x56\\x78...";
//   const size_t kVAR_size = 123;
#define MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
#endif  // OS_WIN

namespace mozc {
namespace codegenstream {
enum StreamOwner {
  NOT_OWN_STREAM,
  OWN_STREAM,
};
}  // namespace codegenstream

class BasicCodeGenByteArrayStreamBuf : public std::streambuf {
 public:
  typedef std::char_traits<char> traits_type;

  // Args:
  //   output_stream: The output stream to which generated code is written.
  //   own_output_stream: The object pointed to by |output_stream| will be
  //       destroyed if |own_output_stream| equals to |OWN_STREAM|.
  BasicCodeGenByteArrayStreamBuf(std::ostream *output_stream,
                                 codegenstream::StreamOwner own_output_stream);

  ~BasicCodeGenByteArrayStreamBuf() override;

  // Writes the beginning of a variable definition.
  bool OpenVarDef(const std::string &var_name_base);

  // Writes the end of a variable definition.
  bool CloseVarDef();

 protected:
  // Flushes.
  int sync() override;

  // Writes a given character sequence.  The implementation is expected to be
  // more efficient than the one of the base class.
  std::streamsize xsputn(const char *s, std::streamsize n) override;

  // Writes the data body of a variable definition.
  int overflow(int c) override;

 private:
  static constexpr size_t kDefaultInternalBufferSize =
      4000 * 1024;  // 4 mega chars
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  static const size_t kNumOfBytesOnOneLine = 4 * sizeof(uint64);
#else
  static constexpr size_t kNumOfBytesOnOneLine = 20;
#endif

  // Converts a raw byte stream to C source code.
  void WriteBytes(const char *begin, const char *end);

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  void WriteWordBuffer();
#endif

  size_t internal_output_buffer_size_;
  std::unique_ptr<char[]> internal_output_buffer_;

  std::basic_ostream<char> *output_stream_;
  codegenstream::StreamOwner own_output_stream_;
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  std::ios_base::fmtflags output_stream_format_flags_;
  char output_stream_format_fill_;
#endif

  bool is_open_;
  std::string var_name_base_;
  size_t output_count_;

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  uint64 word_buffer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BasicCodeGenByteArrayStreamBuf);
};


class CodeGenByteArrayOutputStream : public std::ostream {
 public:
  // Args:
  //   output_stream: The output stream to which generated code is written.
  //   own_output_stream: The object pointed to by |output_stream| will be
  //       destroyed if |own_output_stream| equals to |OWN_STREAM|.
  CodeGenByteArrayOutputStream(std::ostream *output_stream,
                               codegenstream::StreamOwner own_output_stream);

  // Writes the beginning of a variable definition.
  // A call to |OpenVarDef| must precede any output to the instance.
  void OpenVarDef(const std::string &var_name_base);

  // Writes the end of a variable definition.
  // An output to the instance after a call to |CloseVarDef| is not allowed
  // unless |OpenVarDef| is called with a different variable name.
  void CloseVarDef();

 private:
  BasicCodeGenByteArrayStreamBuf streambuf_;

  DISALLOW_COPY_AND_ASSIGN(CodeGenByteArrayOutputStream);
};

}  // namespace mozc

#endif  // MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_
