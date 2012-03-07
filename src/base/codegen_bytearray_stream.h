// Copyright 2010-2012, Google Inc.
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
#include <ostream>
#include <streambuf>
#include <string>

#include "base/base.h"


#ifdef OS_WINDOWS
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
#endif  // OS_WINDOWS

#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
#include <iomanip>
#endif  // MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY

#ifndef MOZC_CODEGEN_BYTEARRAY_STREAM_WORD_TYPE
#define MOZC_CODEGEN_BYTEARRAY_STREAM_WORD_TYPE uint64
#endif  // MOZC_CODEGEN_BYTEARRAY_STREAM_WORD_TYPE

namespace mozc {

namespace codegenstream {
enum StreamOwner {
  NOT_OWN_STREAM,
  OWN_STREAM,
};
}  // namespace codegenstream

template <class charT, class traits = char_traits<charT> >
class BasicCodeGenByteArrayStreamBuf
    : public std::basic_streambuf<charT, traits> {
 public:
  typedef charT char_type;
  typedef typename traits::int_type int_type;
  typedef typename traits::pos_type pos_type;
  typedef typename traits::off_type off_type;
  typedef traits traits_type;

  // Args:
  //   output_stream: The output stream to which generated code is written.
  //   own_output_stream: The object pointed to by |output_stream| will be
  //       destroyed if |own_output_stream| equals to |OWN_STREAM|.
  BasicCodeGenByteArrayStreamBuf(std::basic_ostream<char> *output_stream,
                                 codegenstream::StreamOwner own_output_stream)
      : internal_output_buffer_size_(kDefaultInternalBufferSize),
        internal_output_buffer_(new char_type[internal_output_buffer_size_]),
        output_stream_(output_stream), own_output_stream_(own_output_stream),
        is_open_(false), output_count_(0) {
    this->setp(internal_output_buffer_.get(),
               internal_output_buffer_.get() + internal_output_buffer_size_);
  }

  virtual ~BasicCodeGenByteArrayStreamBuf() {
    CloseVarDef();
    if (own_output_stream_ == codegenstream::OWN_STREAM) {
      delete output_stream_;
    }
  }

  // Writes the beginning of a variable definition.
  bool OpenVarDef(const string &var_name_base) {
    if (is_open_ || var_name_base.empty()) {
      return false;
    }
    var_name_base_ = var_name_base;
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
    *output_stream_ << "const "
                    << AS_STRING(MOZC_CODEGEN_BYTEARRAY_STREAM_WORD_TYPE)
                    << " k" << var_name_base_
                    << "_data_wordtype[] = {\n";
    output_stream_format_flags_ = output_stream_->flags();
    // Set the output format in the form of "0x000012340000ABCD".
    output_stream_->setf(ios_base::hex, ios_base::basefield);
    output_stream_->setf(ios_base::uppercase);
    output_stream_->setf(ios_base::right);
    output_stream_format_fill_ = output_stream_->fill('0');
    // Put the prefix "0x" by ourselves, otherwise it becomes "0X".
    output_stream_->unsetf(ios_base::showbase);
    word_buffer_ = 0;
#else
    *output_stream_ << "const char k" << var_name_base_ << "_data[] =\n";
#endif
    output_count_ = 0;
    return is_open_ = *output_stream_;
  }

  // Writes the end of a variable definition.
  bool CloseVarDef() {
    if (!is_open_) {
      return false;
    }
    overflow();
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
    *output_stream_ << "const size_t k" << var_name_base_ << "_size = "
                    << output_count_ << ";\n";
    is_open_ = false;
    return *output_stream_;
  }

 protected:
  // Flushes.
  virtual int sync() {
    return (overflow() != traits_type::eof() &&
            output_stream_->flush())
        ? 0 : -1;
  }

  // Writes a given character sequence.  The implementation is expected to be
  // more efficient than the one of the base class.
  virtual std::streamsize xsputn(const char_type *s, std::streamsize n) {
    if (n <= this->epptr() - this->pptr()) {
      traits_type::copy(this->pptr(), s, n);
      this->pbump(n);
      return n;
    } else {
      overflow();
      WriteBytes(reinterpret_cast<const char *>(s),
                 reinterpret_cast<const char *>(s + n));
      return n;
    }
  }

  // Writes the data body of a variable definition.
  virtual int_type overflow(int_type c = traits_type::eof()) {
    if (!is_open_) {
      return traits_type::eof();
    }
    if (this->pbase() && this->pptr() && this->pbase() < this->pptr()) {
      WriteBytes(reinterpret_cast<const char *>(this->pbase()),
                 reinterpret_cast<const char *>(this->pptr()));
    }
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
      char_type buf = static_cast<char_type>(c);
      WriteBytes(reinterpret_cast<const char *>(&buf),
                 reinterpret_cast<const char *>(&buf + 1));
    }
    this->setp(internal_output_buffer_.get(),
               internal_output_buffer_.get() + internal_output_buffer_size_);
    return !*output_stream_
        ? traits_type::eof()
        : (traits_type::eq_int_type(c, traits_type::eof())
           ? traits_type::not_eof(c) : c);
  }

 private:
  static const size_t kDefaultInternalBufferSize = 4000 * 1024;  // 4 mega chars
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  typedef MOZC_CODEGEN_BYTEARRAY_STREAM_WORD_TYPE WordType;
  static const size_t kNumOfBytesOnOneLine = 4 * sizeof(WordType);
#else
  static const size_t kNumOfBytesOnOneLine = 20;
#endif

  // Converts a raw byte stream to C source code.
  void WriteBytes(const char *begin, const char *end) {
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
    char * const buf = reinterpret_cast<char *>(&word_buffer_);
    const size_t kWordSize = sizeof word_buffer_;
    while (begin < end) {
      size_t output_length = min(static_cast<size_t>(end - begin),
                                 kWordSize - output_count_ % kWordSize);
      for (size_t i = 0; i < output_length; ++i) {
        buf[output_count_ % kWordSize + i] = *begin++;
      }
      output_count_ += output_length;
      if (output_count_ % kWordSize == 0) {
        WriteWordBuffer();
        *output_stream_ << (output_count_ % kNumOfBytesOnOneLine == 0
                            ? ",\n" : ", ");
      }
    }
#else
    static const char kHex[] = "0123456789ABCDEF";
    while (begin < end) {
      size_t bucket_size = min(static_cast<size_t>(end - begin),
                               kNumOfBytesOnOneLine
                               - output_count_ % kNumOfBytesOnOneLine);
      if (output_count_ % kNumOfBytesOnOneLine == 0) {
        *output_stream_ << '\"';
      }
      for (size_t i = 0; i < bucket_size; ++i) {
        *output_stream_ << "\\x"
                        << kHex[(*begin & 0xF0) >> 4]
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
  void WriteWordBuffer() {
    *output_stream_ << "0x" << setw(2 * sizeof word_buffer_) << word_buffer_;
    word_buffer_ = 0;
  }
#endif

  size_t internal_output_buffer_size_;
  scoped_array<char_type> internal_output_buffer_;

  std::basic_ostream<char> *output_stream_;
  codegenstream::StreamOwner own_output_stream_;
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  ios_base::fmtflags output_stream_format_flags_;
  char_type output_stream_format_fill_;
#endif

  bool is_open_;
  string var_name_base_;
  size_t output_count_;
#ifdef MOZC_CODEGEN_BYTEARRAY_STREAM_USES_WORD_ARRAY
  WordType word_buffer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BasicCodeGenByteArrayStreamBuf);
};

template <class charT, class traits = char_traits<charT> >
class BasicCodeGenByteArrayOutputStream
    : public std::basic_ostream<charT, traits> {
 public:
  typedef charT char_type;
  typedef typename traits::int_type int_type;
  typedef typename traits::pos_type pos_type;
  typedef typename traits::off_type off_type;
  typedef traits traits_type;

  // Args:
  //   output_stream: The output stream to which generated code is written.
  //   own_output_stream: The object pointed to by |output_stream| will be
  //       destroyed if |own_output_stream| equals to |OWN_STREAM|.
  BasicCodeGenByteArrayOutputStream(
      std::basic_ostream<char> *output_stream,
      codegenstream::StreamOwner own_output_stream)
      : std::basic_ostream<char_type, traits_type>(NULL),
        streambuf_(output_stream, own_output_stream) {
    this->rdbuf(&streambuf_);
  }

  // Writes the beginning of a variable definition.
  // A call to |OpenVarDef| must precede any output to the instance.
  void OpenVarDef(const string &var_name_base) {
    if (!streambuf_.OpenVarDef(var_name_base)) {
      this->setstate(ios_base::failbit);
    }
  }

  // Writes the end of a variable definition.
  // An output to the instance after a call to |CloseVarDef| is not allowed
  // unless |OpenVarDef| is called with a different variable name.
  void CloseVarDef() {
    if (!streambuf_.CloseVarDef()) {
      this->setstate(ios_base::failbit);
    }
  }

 private:
  BasicCodeGenByteArrayStreamBuf<char_type, traits_type> streambuf_;

  DISALLOW_COPY_AND_ASSIGN(BasicCodeGenByteArrayOutputStream);
};

typedef BasicCodeGenByteArrayOutputStream<char> CodeGenByteArrayOutputStream;

}  // namespace mozc

#endif  // MOZC_BASE_CODEGEN_BYTEARRAY_STREAM_H_
