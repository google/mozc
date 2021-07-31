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

#include <algorithm>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>

#include "base/port.h"

#ifdef OS_ANDROID
// This is used only for code generation, so shouldn't be used from android
// platform.
#error \
    "base/codegen_bytearray_stream.cc shouldn't be used from android platform."
#endif

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
#include <iomanip>
#endif  // MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY

namespace mozc {
// Args:
//   output_stream: The output stream to which generated code is written.
//   own_output_stream: The object pointed to by |output_stream| will be
//       destroyed if |own_output_stream| equals to |OWN_STREAM|.
BasicCodeGenByteArrayStreamBuf::BasicCodeGenByteArrayStreamBuf(
    std::ostream *output_stream, codegenstream::StreamOwner own_output_stream)
    : internal_output_buffer_size_(kDefaultInternalBufferSize),
      internal_output_buffer_(new char[internal_output_buffer_size_]),
      output_stream_(output_stream),
      own_output_stream_(own_output_stream),
      is_open_(false),
      output_count_(0) {
  this->setp(internal_output_buffer_.get(),
             internal_output_buffer_.get() + internal_output_buffer_size_);
}

BasicCodeGenByteArrayStreamBuf::~BasicCodeGenByteArrayStreamBuf() {
  CloseVarDef();
  if (own_output_stream_ == codegenstream::OWN_STREAM) {
    delete output_stream_;
  }
}

// Writes the beginning of a variable definition.
bool BasicCodeGenByteArrayStreamBuf::OpenVarDef(
    const std::string &var_name_base) {
  if (is_open_ || var_name_base.empty()) {
    return false;
  }
  var_name_base_ = var_name_base;
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  *output_stream_ << "const uint64 k" << var_name_base_
                  << "_data_wordtype[] = {\n";
  output_stream_format_flags_ = output_stream_->flags();
  // Set the output format in the form of "0x000012340000ABCD".
  output_stream_->setf(std::ios_base::hex, std::ios_base::basefield);
  output_stream_->setf(std::ios_base::uppercase);
  output_stream_->setf(std::ios_base::right);
  output_stream_format_fill_ = output_stream_->fill('0');
  // Put the prefix "0x" by ourselves, otherwise it becomes "0X".
  output_stream_->unsetf(std::ios_base::showbase);
  word_buffer_ = 0;
#else
  *output_stream_ << "const char k" << var_name_base_ << "_data[] =\n";
#endif
  output_count_ = 0;
  return is_open_ = !output_stream_->fail();
}

// Writes the end of a variable definition.
bool BasicCodeGenByteArrayStreamBuf::CloseVarDef() {
  if (!is_open_) {
    return false;
  }
  overflow(traits_type::eof());
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  if (output_count_ % sizeof word_buffer_ != 0) {
    // Flush the remaining in |word_buffer_|.
    WriteWordBuffer();
  }
  output_stream_->flags(output_stream_format_flags_);
  output_stream_->fill(output_stream_format_fill_);
  *output_stream_ << "};\n"
                  << "const char * const k" << var_name_base_ << "_data = "
                  << "reinterpret_cast<const char *>("
                  << "k" << var_name_base_ << "_data_wordtype);\n";
#else
  if (output_count_ == 0) {
    *output_stream_ << "\"\"\n";
  } else if (output_count_ % kNumOfBytesOnOneLine != 0) {
    *output_stream_ << "\"\n";
  }
  *output_stream_ << ";\n";
#endif
  *output_stream_ << "const size_t k" << var_name_base_
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
std::streamsize BasicCodeGenByteArrayStreamBuf::xsputn(const char *s,
                                                       std::streamsize n) {
  if (n <= this->epptr() - this->pptr()) {
    traits_type::copy(this->pptr(), s, n);
    this->pbump(n);
    return n;
  } else {
    overflow(traits_type::eof());
    WriteBytes(reinterpret_cast<const char *>(s),
               reinterpret_cast<const char *>(s + n));
    return n;
  }
}

// Writes the data body of a variable definition.
int BasicCodeGenByteArrayStreamBuf::overflow(int c) {
  if (!is_open_) {
    return traits_type::eof();
  }
  if (this->pbase() && this->pptr() && this->pbase() < this->pptr()) {
    WriteBytes(reinterpret_cast<const char *>(this->pbase()),
               reinterpret_cast<const char *>(this->pptr()));
  }
  if (!traits_type::eq_int_type(c, traits_type::eof())) {
    char buf = static_cast<char>(c);
    WriteBytes(reinterpret_cast<const char *>(&buf),
               reinterpret_cast<const char *>(&buf + 1));
  }
  this->setp(internal_output_buffer_.get(),
             internal_output_buffer_.get() + internal_output_buffer_size_);
  return output_stream_->fail()
             ? traits_type::eof()
             : (traits_type::eq_int_type(c, traits_type::eof())
                    ? traits_type::not_eof(c)
                    : c);
}

// Converts a raw byte stream to C source code.
void BasicCodeGenByteArrayStreamBuf::WriteBytes(const char *begin,
                                                const char *end) {
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  char *const buf = reinterpret_cast<char *>(&word_buffer_);
  constexpr size_t kWordSize = sizeof word_buffer_;
  while (begin < end) {
    size_t output_length = std::min(static_cast<size_t>(end - begin),
                                    kWordSize - output_count_ % kWordSize);
    for (size_t i = 0; i < output_length; ++i) {
      buf[output_count_ % kWordSize + i] = *begin++;
    }
    output_count_ += output_length;
    if (output_count_ % kWordSize == 0) {
      WriteWordBuffer();
      *output_stream_ << (output_count_ % kNumOfBytesOnOneLine == 0 ? ",\n"
                                                                    : ", ");
    }
  }
#else
  static constexpr char kHex[] = "0123456789ABCDEF";
  while (begin < end) {
    size_t bucket_size =
        std::min(static_cast<size_t>(end - begin),
                 kNumOfBytesOnOneLine - output_count_ % kNumOfBytesOnOneLine);
    if (output_count_ % kNumOfBytesOnOneLine == 0) {
      *output_stream_ << '\"';
    }
    for (size_t i = 0; i < bucket_size; ++i) {
      *output_stream_ << "\\x" << kHex[(*begin & 0xF0) >> 4]
                      << kHex[(*begin & 0x0F) >> 0];
      ++begin;
    }
    output_count_ += bucket_size;
    if (output_count_ % kNumOfBytesOnOneLine == 0) {
      *output_stream_ << "\"\n";
    }
  }
#endif
}

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
void BasicCodeGenByteArrayStreamBuf::WriteWordBuffer() {
  *output_stream_ << "0x" << std::setw(2 * sizeof word_buffer_) << word_buffer_;
  word_buffer_ = 0;
}
#endif


// Args:
//   output_stream: The output stream to which generated code is written.
//   own_output_stream: The object pointed to by |output_stream| will be
//       destroyed if |own_output_stream| equals to |OWN_STREAM|.
CodeGenByteArrayOutputStream::CodeGenByteArrayOutputStream(
    std::ostream *output_stream, codegenstream::StreamOwner own_output_stream)
    : std::ostream(nullptr), streambuf_(output_stream, own_output_stream) {
  this->rdbuf(&streambuf_);
}

// Writes the beginning of a variable definition.
// A call to |OpenVarDef| must precede any output to the instance.
void CodeGenByteArrayOutputStream::OpenVarDef(
    const std::string &var_name_base) {
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
