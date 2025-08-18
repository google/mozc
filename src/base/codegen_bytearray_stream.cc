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

#include <array>
#include <ios>
#include <ostream>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

#ifdef __ANDROID__
// This is used only for code generation, so shouldn't be used from android
// platform.
#error \
    "base/codegen_bytearray_stream.cc shouldn't be used from android platform."
#endif  // __ANDROID__

namespace mozc {

BasicCodeGenByteArrayStreamBuf::BasicCodeGenByteArrayStreamBuf(
    std::ostream& output_stream)
    : internal_output_buffer_(kDefaultInternalBufferSize),
      output_stream_(&output_stream),
      is_open_(false),
      output_count_(0) {
  this->setp(internal_output_buffer_.data(),
             internal_output_buffer_.data() + internal_output_buffer_.size());
}

// Writes the beginning of a variable definition.
bool BasicCodeGenByteArrayStreamBuf::OpenVarDef(
    const absl::string_view var_name_base) {
  if (is_open_ || var_name_base.empty()) {
    return false;
  }
  var_name_base_.assign(var_name_base.data(), var_name_base.size());
  *output_stream_ << "alignas(std::max_align_t) constexpr char k"
                  << var_name_base_ << "_data[] = {";
  output_count_ = 0;
  return is_open_ = !output_stream_->fail();
}

// Writes the end of a variable definition.
bool BasicCodeGenByteArrayStreamBuf::CloseVarDef() {
  if (!is_open_) {
    return false;
  }
  overflow(traits_type::eof());
  if (output_count_ != 0) {
    *output_stream_ << "\n";
  }
  *output_stream_ << "};\n"
                  << "constexpr size_t k" << var_name_base_
                  << "_size = " << output_count_ << ";\n";
  is_open_ = false;
  return !output_stream_->fail();
}

// Flushes.
int BasicCodeGenByteArrayStreamBuf::sync() {
  return (overflow(traits_type::eof()) != traits_type::eof() &&
          !output_stream_->flush().fail())
             ? 0
             : -1;
}

// Writes a given character sequence.  The implementation is expected to be
// more efficient than the one of the base class.
std::streamsize BasicCodeGenByteArrayStreamBuf::xsputn(const char_type* s,
                                                       std::streamsize n) {
  if (n <= this->epptr() - this->pptr()) {
    traits_type::copy(this->pptr(), s, n);
    this->pbump(n);
    return n;
  } else {
    overflow(traits_type::eof());
    WriteBytes(s, s + n);
    return n;
  }
}

// Writes the data body of a variable definition.
int BasicCodeGenByteArrayStreamBuf::overflow(int c) {
  if (!is_open_) {
    return traits_type::eof();
  }
  if (this->pbase() && this->pptr() && this->pbase() < this->pptr()) {
    WriteBytes(this->pbase(), this->pptr());
  }
  if (!traits_type::eq_int_type(c, traits_type::eof())) {
    const std::array<char, 1> buf = {static_cast<char>(c)};
    WriteBytes(buf.data(), buf.data() + buf.size());
  }
  this->setp(internal_output_buffer_.data(),
             internal_output_buffer_.data() + internal_output_buffer_.size());
  return output_stream_->fail()
             ? traits_type::eof()
             : (traits_type::eq_int_type(c, traits_type::eof())
                    ? traits_type::not_eof(c)
                    : c);
}

// Converts a raw byte stream to C source code.
void BasicCodeGenByteArrayStreamBuf::WriteBytes(const char_type* begin,
                                                const char_type* end) {
  while (begin < end) {
    if (output_count_ % kNumOfBytesOnOneLine == 0) {
      *output_stream_ << absl::StreamFormat("\n0x%02X,", *begin);
    } else {
      *output_stream_ << absl::StreamFormat(" 0x%02X,", *begin);
    }
    ++begin;
    ++output_count_;
  }
}

// Args:
//   output_stream: The output stream to which generated code is written.
//   own_output_stream: The object pointed to by |output_stream| will be
//       destroyed if |own_output_stream| equals to |OWN_STREAM|.
CodeGenByteArrayOutputStream::CodeGenByteArrayOutputStream(
    std::ostream& output_stream)
    : std::ostream(&streambuf_), streambuf_(output_stream) {}

CodeGenByteArrayOutputStream::CodeGenByteArrayOutputStream(
    CodeGenByteArrayOutputStream&& other) noexcept
    : std::ostream(std::move(static_cast<std::ostream&&>(other))),
      streambuf_(std::move(other.streambuf_)) {
  set_rdbuf(&streambuf_);
}
CodeGenByteArrayOutputStream& CodeGenByteArrayOutputStream::operator=(
    CodeGenByteArrayOutputStream&& other) noexcept {
  std::ostream::operator=(std::move(static_cast<std::ostream&&>(other)));
  streambuf_ = std::move(other.streambuf_);
  set_rdbuf(&streambuf_);
  return *this;
}

// Writes the beginning of a variable definition.
// A call to |OpenVarDef| must precede any output to the instance.
void CodeGenByteArrayOutputStream::OpenVarDef(
    const absl::string_view var_name_base) {
  if (!streambuf_.OpenVarDef(var_name_base)) {
    this->setstate(std::ios_base::failbit);
  }
}

// Writes the end of a variable definition.
// An output to the instance after a call to |CloseVarDef| is not allowed
// unless |OpenVarDef| is called with a different variable name.
void CodeGenByteArrayOutputStream::CloseVarDef() {
  if (!streambuf_.CloseVarDef()) {
    this->setstate(std::ios_base::failbit);
  }
}

}  // namespace mozc
